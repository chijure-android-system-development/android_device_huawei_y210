/*
 * libril-y210-shim: adapta libril-qc-1.so (API RIL v6) al libril de CM7.
 *
 * rild carga este shim (rild.libpath) y el shim carga el blob. El blob llena
 * las respuestas con structs v6, el libril de GB las lee con el layout v4/v5.
 *
 * GET_SIM_STATUS: RIL_CardStatus_v6 tiene ims_subscription_app_index entre
 * cdma_subscription_app_index y num_applications (280 bytes contra 276).
 * libril leia ims_index (-1) como num_applications, no escribia ninguna app
 * y la SIM quedaba en UNKNOWN (sin IMSI ni gsm.sim.operator.numeric).
 *
 * GET_IMSI y SIM_IO: en v6 llevan el AID de la aplicacion. libril de GB
 * despacha GET_IMSI con data=NULL (el blob desreferencia NULL en
 * qcril_uim_request_get_imsi) y SIM_IO sin el campo aidPtr final. El shim
 * guarda el AID de la app GSM/USIM de GET_SIM_STATUS y lo agrega.
 */

#define LOG_TAG "RILShim"

#include <dlfcn.h>
#include <pthread.h>
#include <string.h>

#include <cutils/log.h>
#include <telephony/ril.h>

#define REAL_RIL_PATH "/system/lib/libril-qc-1.so"
#define MAX_PENDING   32

typedef struct {
    RIL_CardState card_state;
    RIL_PinState  universal_pin_state;
    int           gsm_umts_subscription_app_index;
    int           cdma_subscription_app_index;
    int           ims_subscription_app_index;
    int           num_applications;
    RIL_AppStatus applications[RIL_CARD_MAX_APPS];
} RIL_CardStatus_v6;

typedef struct {
    int command;
    int fileid;
    char *path;
    int p1;
    int p2;
    int p3;
    char *data;
    char *pin2;
    char *aidPtr;
} RIL_SIM_IO_v6;

/* AID de la app GSM/USIM; no se libera porque el blob puede seguir usandolo */
static char *s_aid;
static pthread_mutex_t s_aidLock = PTHREAD_MUTEX_INITIALIZER;

static const struct RIL_Env *s_rilEnv;
static struct RIL_Env s_shimEnv;
static const RIL_RadioFunctions *s_realFuncs;
static RIL_RadioFunctions s_shimFuncs;

/* tokens de GET_SIM_STATUS pendientes; OnRequestComplete no trae el request */
static RIL_Token s_pending[MAX_PENDING];
static pthread_mutex_t s_pendingLock = PTHREAD_MUTEX_INITIALIZER;

static void pendingAdd(RIL_Token t)
{
    int i;

    pthread_mutex_lock(&s_pendingLock);
    for (i = 0; i < MAX_PENDING; i++) {
        if (s_pending[i] == NULL) {
            s_pending[i] = t;
            break;
        }
    }
    pthread_mutex_unlock(&s_pendingLock);

    if (i == MAX_PENDING)
        LOGW("pendingAdd: tabla llena, token %p sin traducir", t);
}

static int pendingTake(RIL_Token t)
{
    int i, found = 0;

    pthread_mutex_lock(&s_pendingLock);
    for (i = 0; i < MAX_PENDING; i++) {
        if (s_pending[i] == t) {
            s_pending[i] = NULL;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&s_pendingLock);

    return found;
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

static void shimOnRequestComplete(RIL_Token t, RIL_Errno e,
                                  void *response, size_t responselen)
{
    RIL_CardStatus_v6 *v6;
    RIL_CardStatus status;
    int n;

    if (!pendingTake(t) || response == NULL ||
            responselen != sizeof(RIL_CardStatus_v6)) {
        s_rilEnv->OnRequestComplete(t, e, response, responselen);
        return;
    }

    v6 = (RIL_CardStatus_v6 *) response;
    n = v6->num_applications;
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

static void shimOnRequest(int request, void *data, size_t datalen, RIL_Token t)
{
    char *imsiArgs[1];
    RIL_SIM_IO_v6 simIo;

    switch (request) {
    case RIL_REQUEST_GET_SIM_STATUS:
        pendingAdd(t);
        break;

    case RIL_REQUEST_GET_IMSI:
        if (data == NULL) {
            imsiArgs[0] = aidGet();
            data = imsiArgs;
            datalen = sizeof(imsiArgs);
        }
        break;

    case RIL_REQUEST_SIM_IO:
        if (data != NULL && datalen == sizeof(RIL_SIM_IO)) {
            memcpy(&simIo, data, sizeof(RIL_SIM_IO));
            simIo.aidPtr = aidGet();
            data = &simIo;
            datalen = sizeof(simIo);
        }
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

    s_realFuncs = realInit(&s_shimEnv, argc, argv);
    if (s_realFuncs == NULL) {
        LOGE("RIL_Init del blob devolvio NULL");
        return NULL;
    }

    s_shimFuncs = *s_realFuncs;
    s_shimFuncs.onRequest = shimOnRequest;

    LOGI("blob %s cargado (version %d)", REAL_RIL_PATH, s_realFuncs->version);
    return &s_shimFuncs;
}
