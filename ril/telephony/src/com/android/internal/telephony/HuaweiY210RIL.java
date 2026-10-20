/*
 * Copyright (C) 2011 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.internal.telephony;

import static com.android.internal.telephony.RILConstants.*;

import android.content.Context;
import android.os.AsyncResult;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.util.Log;

/**
 * RIL class for the Huawei Y210 (ro.telephony.ril_class=HuaweiY210RIL).
 * Lives in the device tree as y210-telephony.jar on the BOOTCLASSPATH.
 *
 * Custom Qualcomm RIL for Huawei devices.
 * Extends HuaweiRIL (CM9 base) and passes mAid in facility-lock requests.
 *
 * The Y210's libril-qc-1 is a v6 Qualcomm RIL: radio state 2 means "radio on",
 * not SIM_NOT_READY, and it never reports SIM_READY by itself. Like
 * QualcommSharedRIL, derive the SIM radio state from GET_SIM_STATUS whenever
 * the radio comes on or the RIL reports a SIM status change; otherwise the
 * framework never leaves NOT_READY (no SIM records, SMS, operator name, mAid).
 * {@hide}
 */
public class HuaweiY210RIL extends HuaweiRIL {

    private static final int EVENT_ICC_STATUS_CHANGED = 1;
    private static final int EVENT_GET_ICC_STATUS_DONE = 2;

    private Handler mIccHandler;

    public HuaweiY210RIL(Context context, int networkMode, int cdmaSubscription) {
        super(context, networkMode, cdmaSubscription);
    }

    @Override
    public void
    getIMSI(Message result) {
        // qcril_uim_request_get_imsi dereferences data[0] (the AID); HuaweiRIL's
        // bare writeString(null) has no array count and makes rild SIGSEGV.
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_GET_IMSI, result);

        rr.mp.writeInt(1);
        rr.mp.writeString(mAid);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest)
                    + " aid: " + mAid);

        send(rr);
    }

    @Override
    public void
    setupDataCall(String radioTechnology, String profile, String apn,
            String user, String password, String authType, String protocol,
            Message result) {
        // HuaweiRIL hardcodes "0" as the 7th string; qcril expects the PDP
        // protocol ("IP", "IPV6", "IPV4V6") and fails with "invalid ip family".
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SETUP_DATA_CALL, result);

        rr.mp.writeInt(7);

        rr.mp.writeString(radioTechnology);
        rr.mp.writeString(profile);
        rr.mp.writeString(apn);
        rr.mp.writeString(user);
        rr.mp.writeString(password);
        rr.mp.writeString(authType);
        rr.mp.writeString(protocol);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> "
                    + requestToString(rr.mRequest) + " " + radioTechnology + " "
                    + profile + " " + apn + " " + user + " "
                    + password + " " + authType + " " + protocol);

        send(rr);
    }

    @Override
    public void
    iccIO(int command, int fileid, String path, int p1, int p2, int p3,
            String data, String pin2, Message result) {
        // ICS libril (dispatchSIM_IO, RIL_SIM_IO_v6) reads the AID last;
        // HuaweiRIL writes it first, which shifts every field and makes rild
        // SIGSEGV formatting the garbage path/data pointers.
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SIM_IO, result);

        rr.mp.writeInt(command);
        rr.mp.writeInt(fileid);
        rr.mp.writeString(path);
        rr.mp.writeInt(p1);
        rr.mp.writeInt(p2);
        rr.mp.writeInt(p3);
        rr.mp.writeString(data);
        rr.mp.writeString(pin2);
        rr.mp.writeString(mAid);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> iccIO: "
                    + requestToString(rr.mRequest)
                    + " 0x" + Integer.toHexString(command)
                    + " 0x" + Integer.toHexString(fileid) + " "
                    + " path: " + path + ","
                    + p1 + "," + p2 + "," + p3
                    + " aid: " + mAid);

        send(rr);
    }

    @Override
    protected void switchToRadioState(RadioState newState) {
        setRadioState(newState);

        if (newState == RadioState.SIM_NOT_READY) {
            if (mIccHandler == null) {
                HandlerThread thread = new HandlerThread("HuaweiIccHandler");
                thread.start();
                mIccHandler = new IccHandler(thread);
                registerForIccStatusChanged(mIccHandler, EVENT_ICC_STATUS_CHANGED, null);
            }
            mIccHandler.sendEmptyMessage(EVENT_ICC_STATUS_CHANGED);
        }
    }

    private class IccHandler extends Handler {
        IccHandler(HandlerThread thread) {
            super(thread.getLooper());
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case EVENT_ICC_STATUS_CHANGED:
                    if (getRadioState().isOn()) {
                        getIccCardStatus(obtainMessage(EVENT_GET_ICC_STATUS_DONE));
                    }
                    break;
                case EVENT_GET_ICC_STATUS_DONE:
                    onIccStatus((AsyncResult) msg.obj);
                    break;
            }
        }
    }

    private void onIccStatus(AsyncResult ar) {
        if (ar.exception != null) {
            Log.e(LOG_TAG, "GET_SIM_STATUS failed", ar.exception);
            return;
        }
        if (!getRadioState().isOn()) {
            return;
        }

        IccCardStatus status = (IccCardStatus) ar.result;
        int appIndex = status.getGsmUmtsSubscriptionAppIndex();
        if (status.getNumApplications() == 0 || appIndex < 0) {
            setRadioState(RadioState.SIM_LOCKED_OR_ABSENT);
            return;
        }

        IccCardApplication app = status.getApplication(appIndex);
        Log.d(LOG_TAG, "SIM app state " + app.app_state + " type " + app.app_type);
        switch (app.app_state) {
            case APPSTATE_READY:
                setRadioState(RadioState.SIM_READY);
                break;
            case APPSTATE_PIN:
            case APPSTATE_PUK:
            case APPSTATE_SUBSCRIPTION_PERSO:
                setRadioState(RadioState.SIM_LOCKED_OR_ABSENT);
                break;
            default:
                // DETECTED/UNKNOWN: still initialising, wait for SIM_STATUS_CHANGED.
                break;
        }
    }

    @Override
    public void
    queryFacilityLock(String facility, String password, int serviceClass,
                      Message response) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_QUERY_FACILITY_LOCK, response);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest)
                    + " aid: " + mAid + " facility: " + facility);

        rr.mp.writeInt(4);
        rr.mp.writeString(facility);
        rr.mp.writeString(password);
        rr.mp.writeString(Integer.toString(serviceClass));
        rr.mp.writeString(mAid);

        send(rr);
    }

    @Override
    public void
    setFacilityLock(String facility, boolean lockState, String password,
                    int serviceClass, Message response) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SET_FACILITY_LOCK, response);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest)
                    + " aid: " + mAid + " facility: " + facility
                    + " lockstate: " + lockState);

        rr.mp.writeInt(5);
        rr.mp.writeString(facility);
        rr.mp.writeString(lockState ? "1" : "0");
        rr.mp.writeString(password);
        rr.mp.writeString(Integer.toString(serviceClass));
        rr.mp.writeString(mAid);

        send(rr);
    }

    /*
     * PIN/PUK in the standard ICS format ({n strings, aid}). HuaweiRIL sends
     * {null, pin...} for other Huawei RILs: libril reads the null as a string
     * count of -1, hands the blob a NULL array and qcril_uim_request_enter_pin
     * crashes (rild restarts; the keyguard says the PIN is wrong).
     */
    private void sendPinStrings(int request, Message result, String... args) {
        RILRequest rr = RILRequest.obtain(request, result);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest));

        rr.mp.writeInt(args.length);
        for (String arg : args) {
            rr.mp.writeString(arg);
        }

        send(rr);
    }

    private String aidOrSim(String aid) {
        return aid != null ? aid : mAid;
    }

    @Override
    public void
    supplyIccPin(String pin, Message result) {
        sendPinStrings(RIL_REQUEST_ENTER_SIM_PIN, result, pin, mAid);
    }

    @Override
    public void
    supplyIccPinForApp(String pin, String aid, Message result) {
        sendPinStrings(RIL_REQUEST_ENTER_SIM_PIN, result, pin, aidOrSim(aid));
    }

    @Override
    public void
    supplyIccPin2ForApp(String pin, String aid, Message result) {
        sendPinStrings(RIL_REQUEST_ENTER_SIM_PIN2, result, pin, aidOrSim(aid));
    }

    @Override
    public void
    supplyIccPukForApp(String puk, String newPin, String aid, Message result) {
        sendPinStrings(RIL_REQUEST_ENTER_SIM_PUK, result, puk, newPin, aidOrSim(aid));
    }

    @Override
    public void
    supplyIccPuk2ForApp(String puk, String newPin2, String aid, Message result) {
        sendPinStrings(RIL_REQUEST_ENTER_SIM_PUK2, result, puk, newPin2, aidOrSim(aid));
    }

    @Override
    public void
    changeIccPinForApp(String oldPin, String newPin, String aid, Message result) {
        sendPinStrings(RIL_REQUEST_CHANGE_SIM_PIN, result, oldPin, newPin, aidOrSim(aid));
    }

    @Override
    public void
    changeIccPin2ForApp(String oldPin2, String newPin2, String aid, Message result) {
        sendPinStrings(RIL_REQUEST_CHANGE_SIM_PIN2, result, oldPin2, newPin2, aidOrSim(aid));
    }
}
