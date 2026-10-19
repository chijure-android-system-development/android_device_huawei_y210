/*
 * libril-y210-shim: adapta libril-qc-1.so (API RIL v6) al libril de CM7.
 *
 * rild carga este shim (rild.libpath) y el shim carga el blob. El blob usa
 * los structs v6 del libril.so stock (Huawei/CAF), el libril de GB los lee
 * con el layout v4/v5. Los layouts salen de desensamblar el libril.so stock.
 *
 * GET_SIM_STATUS: RIL_CardStatus_v6 tiene ims_subscription_app_index entre
 * cdma_subscription_app_index y num_applications (280 bytes contra 276).
 * libril leia ims_index (-1) como num_applications, no escribia ninguna app
 * y la SIM quedaba en UNKNOWN (sin IMSI ni gsm.sim.operator.numeric).
 *
 * GET_IMSI: en v6 lleva el AID de la aplicacion. libril de GB lo despacha
 * con data=NULL y el blob desreferencia NULL en qcril_uim_request_get_imsi.
 * El shim guarda el AID de la app GSM/USIM de GET_SIM_STATUS y lo agrega.
 *
 * PIN/PUK (ENTER_SIM_PIN/PUK/PIN2/PUK2, CHANGE_SIM_PIN/PIN2): en v6 llevan
 * el AID como ultimo string (el RIL.java stock de Huawei lo agrega). El de GB
 * no: ENTER_SIM_PIN con un solo string volvia REQUEST_NOT_SUPPORTED sin llegar
 * a la SIM y el keyguard decia "PIN incorrecto". El shim agrega el AID.
 *
 * SIM_IO: dispatchSIM_IO del stock hace memset de 40 bytes, deja la primera
 * palabra en 0, pone los campos de GB desde el offset 4 y aidPtr en el 36;
 * datalen 40 para version > 5. Con el RIL_SIM_IO de GB el blob leia el AID
 * de la pila de libril y crasheaba en vfprintf.
 *
 * SETUP_DATA_CALL / DATA_CALL_LIST / UNSOL_DATA_CALL_LIST_CHANGED: el blob
 * responde con {status, cid, active, type, ifname, addresses, dnses,
 * gateways} (32 bytes). GB espera {cid, ifname, ip} en SETUP_DATA_CALL (dns y
 * gw los lee de net.<ifname>.*) y RIL_Data_Call_Response en las listas.
 *
 * Version y unsolicited: el shim se registra con RIL_VERSION. El blob manda
 * codigos v6 > RIL_UNSOL_RESEND_INCALL_MUTE que GB no conoce; 1037 (cambio de
 * estado de la red de datos) se reenvia como UNSOL_RESPONSE_NETWORK_STATE_
 * CHANGED y el resto se descarta.
 */

#define LOG_TAG "RILShim"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cutils/log.h>
#include <telephony/ril.h>

#define REAL_RIL_PATH "/system/lib/libril-qc-1.so"
#define MAX_PENDING   64
#define MAX_DATA_CALLS 8

#define QC_UNSOL_DATA_NETWORK_STATE_CHANGED 1037

typedef struct {
    RIL_CardState card_state;
    RIL_PinState  universal_pin_state;
    int           gsm_umts_subscription_app_index;
    int           cdma_subscription_app_index;
    int           ims_subscription_app_index;
    int           num_applications;
    RIL_AppStatus applications[RIL_CARD_MAX_APPS];
} RIL_CardStatus_v6;

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

/* data call del blob, layout de responseDataCallList del stock (32 bytes) */
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
        LOGW("pendingAdd: tabla llena, token %p sin seguimiento", t);
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

static void aidUpdate(const RIL_CardStatus_v6 *v6, int numApps)
{
    const char *aid;
    int idx = v6->gsm_umts_subscription_app_index;

    if (idx < 0 || idx >= numApps)
        return;

    aid = v6->applications[idx].aid_ptr;
    if (aid == NULL)
        return;

    pthread_mutex_lock(&s_aidLock);
    if (s_aid == NULL || strcmp(s_aid, aid) != 0) {
        s_aid = strdup(aid);
        LOGD("AID GSM/USIM: %s", s_aid);
    }
    pthread_mutex_unlock(&s_aidLock);
}

static void completeSimStatus(RIL_Token t, RIL_Errno e,
                              const RIL_CardStatus_v6 *v6)
{
    RIL_CardStatus status;
    int n = v6->num_applications;

    if (n < 0)
        n = 0;
    if (n > RIL_CARD_MAX_APPS)
        n = RIL_CARD_MAX_APPS;

    memset(&status, 0, sizeof(status));
    status.card_state = v6->card_state;
    status.universal_pin_state = v6->universal_pin_state;
    status.gsm_umts_subscription_app_index = v6->gsm_umts_subscription_app_index;
    status.cdma_subscription_app_index = v6->cdma_subscription_app_index;
    status.num_applications = n;
    memcpy(status.applications, v6->applications, n * sizeof(RIL_AppStatus));
    aidUpdate(v6, n);

    LOGD("GET_SIM_STATUS v6->v5: card=%d gsm=%d cdma=%d ims=%d apps=%d",
         v6->card_state, v6->gsm_umts_subscription_app_index,
         v6->cdma_subscription_app_index, v6->ims_subscription_app_index, n);

    s_rilEnv->OnRequestComplete(t, e, &status, sizeof(status));
}

/* SETUP_DATA_CALL v6 -> {cid, ifname, ip} de GB */
static void completeSetupDataCall(RIL_Token t, RIL_Errno e,
                                  const RIL_Data_Call_Response_qc *dc)
{
    char cid[12], ip[64];
    char *strings[3];
    size_t len;

    if (dc->status != 0) {
        LOGW("SETUP_DATA_CALL: status=%d", dc->status);
        s_rilEnv->OnRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    /* addresses puede traer varias "ip/prefijo" separadas por espacio */
    ip[0] = '\0';
    if (dc->addresses != NULL) {
        len = strcspn(dc->addresses, " /");
        if (len >= sizeof(ip))
            len = sizeof(ip) - 1;
        memcpy(ip, dc->addresses, len);
        ip[len] = '\0';
    }

    snprintf(cid, sizeof(cid), "%d", dc->cid);
    strings[0] = cid;
    strings[1] = dc->ifname;
    strings[2] = ip;

    LOGD("SETUP_DATA_CALL v6->v4: cid=%s ifname=%s ip=%s active=%d",
         cid, dc->ifname != NULL ? dc->ifname : "(null)", ip, dc->active);

    s_rilEnv->OnRequestComplete(t, e, strings, sizeof(strings));
}

/* lista de data calls v6 -> RIL_Data_Call_Response de GB; 0 si no aplica */
static int dataCallListToV4(const void *data, size_t datalen,
                            RIL_Data_Call_Response *out, size_t *outlen)
{
    const RIL_Data_Call_Response_qc *dc = data;
    size_t i, n;

    if (datalen % sizeof(RIL_Data_Call_Response_qc) != 0)
        return 0;

    n = datalen / sizeof(RIL_Data_Call_Response_qc);
    if (n > MAX_DATA_CALLS)
        n = MAX_DATA_CALLS;

    for (i = 0; i < n; i++) {
        out[i].cid = dc[i].cid;
        out[i].active = dc[i].active;
        out[i].type = dc[i].type;
        out[i].apn = NULL;
        out[i].address = dc[i].addresses;
    }

    *outlen = n * sizeof(RIL_Data_Call_Response);
    return 1;
}

static void shimOnRequestComplete(RIL_Token t, RIL_Errno e,
                                  void *response, size_t responselen)
{
    RIL_Data_Call_Response list[MAX_DATA_CALLS];
    size_t listlen;
    void *owned;
    int request = pendingTake(t, &owned);

    if (response == NULL || e != RIL_E_SUCCESS) {
        s_rilEnv->OnRequestComplete(t, e, response, responselen);
    } else if (request == RIL_REQUEST_GET_SIM_STATUS &&
               responselen == sizeof(RIL_CardStatus_v6)) {
        completeSimStatus(t, e, response);
    } else if (request == RIL_REQUEST_SETUP_DATA_CALL &&
               responselen == sizeof(RIL_Data_Call_Response_qc)) {
        completeSetupDataCall(t, e, response);
    } else if (request == RIL_REQUEST_DATA_CALL_LIST &&
               dataCallListToV4(response, responselen, list, &listlen)) {
        s_rilEnv->OnRequestComplete(t, e, listlen ? list : NULL, listlen);
    } else {
        s_rilEnv->OnRequestComplete(t, e, response, responselen);
    }

    free(owned);
}

static void shimOnUnsolicitedResponse(int unsolResponse, const void *data,
                                      size_t datalen)
{
    RIL_Data_Call_Response list[MAX_DATA_CALLS];
    size_t listlen;

    if (unsolResponse == RIL_UNSOL_DATA_CALL_LIST_CHANGED && data != NULL &&
            dataCallListToV4(data, datalen, list, &listlen)) {
        s_rilEnv->OnUnsolicitedResponse(unsolResponse,
                                        listlen ? list : NULL, listlen);
        return;
    }

    if (unsolResponse == QC_UNSOL_DATA_NETWORK_STATE_CHANGED) {
        s_rilEnv->OnUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
        return;
    }

    /* codigos v6 que el libril de GB no tiene en s_unsolResponses */
    if (unsolResponse > RIL_UNSOL_RESEND_INCALL_MUTE)
        return;

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

/* RIL_SIM_IO (GB) -> RIL_SIM_IO_qc, struct y strings en un bloque de heap */
static RIL_SIM_IO_qc *simIoToQc(const RIL_SIM_IO *in, const char *aid)
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
static char **imsiArgsV6(const char *aid)
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

/* PIN/PUK v6: los strings de GB con el AID agregado al final */
static char **stringsWithAid(char **in, int n, const char *aid)
{
    char **out;
    char *str;
    size_t size = (n + 1) * sizeof(char *) + strSize(aid);
    int i;

    for (i = 0; i < n; i++)
        size += strSize(in[i]);
    out = malloc(size);
    if (out == NULL)
        return NULL;

    str = (char *) (out + n + 1);
    for (i = 0; i < n; i++)
        out[i] = copyStr(&str, in[i]);
    out[n] = copyStr(&str, aid);

    return out;
}

/* cantidad de strings que manda el RIL.java de GB, sin AID */
static int pinRequestStrings(int request)
{
    switch (request) {
    case RIL_REQUEST_ENTER_SIM_PIN:
    case RIL_REQUEST_ENTER_SIM_PIN2:
        return 1;
    case RIL_REQUEST_ENTER_SIM_PUK:
    case RIL_REQUEST_ENTER_SIM_PUK2:
    case RIL_REQUEST_CHANGE_SIM_PIN:
    case RIL_REQUEST_CHANGE_SIM_PIN2:
        return 2;
    }
    return 0;
}

static void shimOnRequest(int request, void *data, size_t datalen, RIL_Token t)
{
    void *owned = NULL;
    int n;

    switch (request) {
    case RIL_REQUEST_GET_SIM_STATUS:
    case RIL_REQUEST_SETUP_DATA_CALL:
    case RIL_REQUEST_DATA_CALL_LIST:
        pendingAdd(t, request, NULL);
        break;

    case RIL_REQUEST_GET_IMSI:
        if (data == NULL) {
            owned = imsiArgsV6(aidGet());
            if (owned != NULL) {
                data = owned;
                datalen = sizeof(char *);
            }
        }
        pendingAdd(t, request, owned);
        break;

    case RIL_REQUEST_ENTER_SIM_PIN:
    case RIL_REQUEST_ENTER_SIM_PUK:
    case RIL_REQUEST_ENTER_SIM_PIN2:
    case RIL_REQUEST_ENTER_SIM_PUK2:
    case RIL_REQUEST_CHANGE_SIM_PIN:
    case RIL_REQUEST_CHANGE_SIM_PIN2:
        n = pinRequestStrings(request);
        if (data != NULL && datalen == n * sizeof(char *) &&
                aidGet() != NULL) {
            owned = stringsWithAid((char **) data, n, aidGet());
            if (owned != NULL) {
                data = owned;
                datalen = (n + 1) * sizeof(char *);
            }
        }
        pendingAdd(t, request, owned);
        break;

    case RIL_REQUEST_SIM_IO:
        if (data != NULL && datalen == sizeof(RIL_SIM_IO)) {
            owned = simIoToQc((const RIL_SIM_IO *) data, aidGet());
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
        LOGE("dlopen %s: %s", REAL_RIL_PATH, dlerror());
        return NULL;
    }

    realInit = dlsym(handle, "RIL_Init");
    if (realInit == NULL) {
        LOGE("RIL_Init no encontrado en %s", REAL_RIL_PATH);
        return NULL;
    }

    s_rilEnv = env;
    s_shimEnv = *env;
    s_shimEnv.OnRequestComplete = shimOnRequestComplete;
    s_shimEnv.OnUnsolicitedResponse = shimOnUnsolicitedResponse;

    s_realFuncs = realInit(&s_shimEnv, argc, argv);
    if (s_realFuncs == NULL) {
        LOGE("RIL_Init del blob devolvio NULL");
        return NULL;
    }

    /* libril de GB recibe un RIL v3; las diferencias v6 las absorbe el shim */
    s_shimFuncs = *s_realFuncs;
    s_shimFuncs.version = RIL_VERSION;
    s_shimFuncs.onRequest = shimOnRequest;

    LOGI("blob %s cargado (version %d)", REAL_RIL_PATH, s_realFuncs->version);
    return &s_shimFuncs;
}
