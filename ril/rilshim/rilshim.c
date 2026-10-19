/*
 * libril-y210-shim: adapta libril-qc-1.so al libril de ICS/JB (CM10 conserva los mismos layouts).
 *
 * rild carga este shim (rild.libpath) y el shim carga el blob. El blob
 * declara RIL v6 pero usa los layouts del libril.so stock (Huawei/CAF),
 * que no coinciden con los de ICS. Los layouts salen de desensamblar el
 * blob y el libril.so stock (ver RIL_NOTES.md y el shim de CM7).
 *
 * GET_IMSI: el blob lee el AID de data[0]. ICS despacha GET_IMSI con
 * dispatchVoid (data=NULL) y el blob hace SIGSEGV en
 * qcril_uim_request_get_imsi. El shim guarda el AID de la app GSM/USIM de
 * GET_SIM_STATUS y lo agrega.
 *
 * SIM_IO: el blob lee RIL_SIM_IO_v6 corrido una palabra (command en +4,
 * aidPtr en +36, 40 bytes). Con el struct de ICS leia el AID de la pila de
 * libril y crasheaba en vfprintf.
 *
 * SETUP_DATA_CALL / DATA_CALL_LIST / UNSOL_DATA_CALL_LIST_CHANGED: el blob
 * responde {status, cid, active, type, ifname, addresses, dnses, gateways}
 * (32 bytes, sin suggestedRetryTime); ICS espera RIL_Data_Call_Response_v6.
 */

#define LOG_TAG "RILShim"

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <cutils/log.h>
#include <telephony/ril.h>

#define REAL_RIL_PATH  "/system/lib/libril-qc-1.so"
#define MAX_PENDING    64
#define MAX_DATA_CALLS 8

/* RIL_SIM_IO como lo arma el libril.so stock para el blob (40 bytes) */
typedef struct {
    int leading;    /* el libril stock nunca lo escribe: queda en 0 */
    int command;
    int fileid;
    char *path;
    int p1;
    int p2;
    int p3;
    char *data;
    char *pin2;
    char *aidPtr;
} RIL_SIM_IO_qc;

/* data call del blob (32 bytes) */
typedef struct {
    int status;
    int cid;
    int active;
    char *type;
    char *ifname;
    char *addresses;
    char *dnses;
    char *gateways;
} RIL_Data_Call_Response_qc;

/* AID de la app GSM/USIM; no se libera porque el blob puede seguir usandolo */
static char *s_aid;
static pthread_mutex_t s_aidLock = PTHREAD_MUTEX_INITIALIZER;

static const struct RIL_Env *s_rilEnv;
static struct RIL_Env s_shimEnv;
static const RIL_RadioFunctions *s_realFuncs;
static RIL_RadioFunctions s_shimFuncs;

/*
 * requests pendientes; OnRequestComplete no trae el request. "owned" es la
 * copia en heap de los parametros que el shim le paso al blob: qcril los usa
 * desde su propio hilo despues de que onRequest retorna, asi que viven hasta
 * la respuesta.
 */
static struct {
    RIL_Token t;
    int request;
    void *owned;
} s_pending[MAX_PENDING];
static pthread_mutex_t s_pendingLock = PTHREAD_MUTEX_INITIALIZER;

static void pendingAdd(RIL_Token t, int request, void *owned)
{
    int i;

    pthread_mutex_lock(&s_pendingLock);
    for (i = 0; i < MAX_PENDING; i++) {
        if (s_pending[i].t == NULL) {
            s_pending[i].t = t;
            s_pending[i].request = request;
            s_pending[i].owned = owned;
            break;
        }
    }
    pthread_mutex_unlock(&s_pendingLock);

    if (i == MAX_PENDING)
        ALOGW("pendingAdd: tabla llena, token %p sin seguimiento", t);
}

static int pendingTake(RIL_Token t, void **owned)
{
    int i, request = -1;

    *owned = NULL;

    pthread_mutex_lock(&s_pendingLock);
    for (i = 0; i < MAX_PENDING; i++) {
        if (s_pending[i].t == t) {
            s_pending[i].t = NULL;
            request = s_pending[i].request;
            *owned = s_pending[i].owned;
            break;
        }
    }
    pthread_mutex_unlock(&s_pendingLock);

    return request;
}

static char *aidGet(void)
{
    char *aid;

    pthread_mutex_lock(&s_aidLock);
    aid = s_aid;
    pthread_mutex_unlock(&s_aidLock);

    return aid;
}

static void aidUpdate(const RIL_CardStatus_v6 *status)
{
    const char *aid;
    int idx = status->gsm_umts_subscription_app_index;

    if (idx < 0 || idx >= status->num_applications || idx >= RIL_CARD_MAX_APPS)
        return;

    aid = status->applications[idx].aid_ptr;
    if (aid == NULL)
        return;

    pthread_mutex_lock(&s_aidLock);
    if (s_aid == NULL || strcmp(s_aid, aid) != 0) {
        s_aid = strdup(aid);
        ALOGD("AID GSM/USIM: %s", s_aid);
    }
    pthread_mutex_unlock(&s_aidLock);
}

/* data calls del blob -> RIL_Data_Call_Response_v6; 0 si no aplica */
static int dataCallsToV6(const void *data, size_t datalen,
                         RIL_Data_Call_Response_v6 *out, size_t *outlen)
{
    const RIL_Data_Call_Response_qc *dc = data;
    size_t i, n;

    if (datalen % sizeof(RIL_Data_Call_Response_qc) != 0)
        return 0;

    n = datalen / sizeof(RIL_Data_Call_Response_qc);
    if (n > MAX_DATA_CALLS)
        n = MAX_DATA_CALLS;

    for (i = 0; i < n; i++) {
        out[i].status = dc[i].status;
        out[i].suggestedRetryTime = -1;
        out[i].cid = dc[i].cid;
        out[i].active = dc[i].active;
        out[i].type = dc[i].type;
        out[i].ifname = dc[i].ifname;
        out[i].addresses = dc[i].addresses;
        out[i].dnses = dc[i].dnses;
        out[i].gateways = dc[i].gateways;
    }

    *outlen = n * sizeof(RIL_Data_Call_Response_v6);
    return 1;
}

static void shimOnRequestComplete(RIL_Token t, RIL_Errno e,
                                  void *response, size_t responselen)
{
    RIL_Data_Call_Response_v6 list[MAX_DATA_CALLS];
    size_t listlen;
    void *owned;
    int request = pendingTake(t, &owned);

    if (request == RIL_REQUEST_GET_SIM_STATUS && response != NULL &&
            responselen == sizeof(RIL_CardStatus_v6))
        aidUpdate(response);

    if (response != NULL &&
            (request == RIL_REQUEST_SETUP_DATA_CALL ||
             request == RIL_REQUEST_DATA_CALL_LIST) &&
            dataCallsToV6(response, responselen, list, &listlen)) {
        if (request == RIL_REQUEST_SETUP_DATA_CALL && listlen > 0)
            ALOGD("SETUP_DATA_CALL: status=%d cid=%d ifname=%s addr=%s",
                 list[0].status, list[0].cid,
                 list[0].ifname != NULL ? list[0].ifname : "(null)",
                 list[0].addresses != NULL ? list[0].addresses : "(null)");
        s_rilEnv->OnRequestComplete(t, e, listlen ? list : NULL, listlen);
    } else {
        s_rilEnv->OnRequestComplete(t, e, response, responselen);
    }

    free(owned);
}

static void shimOnUnsolicitedResponse(int unsolResponse, const void *data,
                                      size_t datalen)
{
    RIL_Data_Call_Response_v6 list[MAX_DATA_CALLS];
    size_t listlen;

    if (unsolResponse == RIL_UNSOL_DATA_CALL_LIST_CHANGED && data != NULL &&
            dataCallsToV6(data, datalen, list, &listlen)) {
        s_rilEnv->OnUnsolicitedResponse(unsolResponse,
                                        listlen ? list : NULL, listlen);
        return;
    }

    s_rilEnv->OnUnsolicitedResponse(unsolResponse, data, datalen);
}

/* copia un string dentro del bloque "dst"; devuelve el puntero copiado */
static char *copyStr(char **dst, const char *src)
{
    char *ret;
    size_t len;

    if (src == NULL)
        return NULL;

    len = strlen(src) + 1;
    ret = *dst;
    memcpy(ret, src, len);
    *dst += len;

    return ret;
}

static size_t strSize(const char *str)
{
    return str != NULL ? strlen(str) + 1 : 0;
}

/* RIL_SIM_IO_v6 (ICS) -> RIL_SIM_IO_qc, struct y strings en un bloque de heap */
static RIL_SIM_IO_qc *simIoToQc(const RIL_SIM_IO_v6 *in, const char *aid)
{
    RIL_SIM_IO_qc *out;
    char *str;

    out = malloc(sizeof(*out) + strSize(in->path) + strSize(in->data) +
                 strSize(in->pin2) + strSize(aid));
    if (out == NULL)
        return NULL;

    str = (char *) (out + 1);
    out->leading = 0;
    out->command = in->command;
    out->fileid = in->fileid;
    out->path = copyStr(&str, in->path);
    out->p1 = in->p1;
    out->p2 = in->p2;
    out->p3 = in->p3;
    out->data = copyStr(&str, in->data);
    out->pin2 = copyStr(&str, in->pin2);
    out->aidPtr = copyStr(&str, aid);

    return out;
}

/* GET_IMSI v6: char *[1] = { aid } */
static char **imsiArgs(const char *aid)
{
    char **out;
    char *str;

    out = malloc(sizeof(char *) + strSize(aid));
    if (out == NULL)
        return NULL;

    str = (char *) (out + 1);
    out[0] = copyStr(&str, aid);

    return out;
}

static void shimOnRequest(int request, void *data, size_t datalen, RIL_Token t)
{
    void *owned = NULL;

    switch (request) {
    case RIL_REQUEST_GET_SIM_STATUS:
    case RIL_REQUEST_SETUP_DATA_CALL:
    case RIL_REQUEST_DATA_CALL_LIST:
        pendingAdd(t, request, NULL);
        break;

    case RIL_REQUEST_GET_IMSI:
        if (data == NULL) {
            owned = imsiArgs(aidGet());
            if (owned != NULL) {
                data = owned;
                datalen = sizeof(char *);
            }
        }
        pendingAdd(t, request, owned);
        break;

    case RIL_REQUEST_SIM_IO:
        if (data != NULL && datalen == sizeof(RIL_SIM_IO_v6)) {
            const RIL_SIM_IO_v6 *in = data;
            owned = simIoToQc(in, in->aidPtr != NULL ? in->aidPtr : aidGet());
            if (owned != NULL) {
                data = owned;
                datalen = sizeof(RIL_SIM_IO_qc);
            }
        }
        pendingAdd(t, request, owned);
        break;
    }

    s_realFuncs->onRequest(request, data, datalen, t);
}

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    const RIL_RadioFunctions *(*realInit)(const struct RIL_Env *, int, char **);
    void *handle;

    handle = dlopen(REAL_RIL_PATH, RTLD_NOW);
    if (handle == NULL) {
        ALOGE("dlopen %s: %s", REAL_RIL_PATH, dlerror());
        return NULL;
    }

    realInit = dlsym(handle, "RIL_Init");
    if (realInit == NULL) {
        ALOGE("RIL_Init no encontrado en %s", REAL_RIL_PATH);
        return NULL;
    }

    s_rilEnv = env;
    s_shimEnv = *env;
    s_shimEnv.OnRequestComplete = shimOnRequestComplete;
    s_shimEnv.OnUnsolicitedResponse = shimOnUnsolicitedResponse;

    s_realFuncs = realInit(&s_shimEnv, argc, argv);
    if (s_realFuncs == NULL) {
        ALOGE("RIL_Init del blob devolvio NULL");
        return NULL;
    }

    s_shimFuncs = *s_realFuncs;
    s_shimFuncs.onRequest = shimOnRequest;

    ALOGI("blob %s cargado (version %d)", REAL_RIL_PATH, s_realFuncs->version);
    return &s_shimFuncs;
}
