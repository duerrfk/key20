/**
 * This file is part of Key20.
 *
 * Copyright 2016 Frank Duerr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package de.frank_durr.key20;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Message;
import android.support.design.widget.TabLayout;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;
import android.support.v7.app.AlertDialog;
import android.os.Bundle;
import android.support.v7.widget.Toolbar;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.util.Base64;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import java.lang.ref.WeakReference;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.security.Security;
import java.util.UUID;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

import de.frank_durr.ecdh_curve25519.ECDHCurve25519;

public class MainActivity extends android.support.v7.app.AppCompatActivity {

    public static final String TAG = MainActivity.class.getName();

    static {
        // We use the Bouncy Castle crypto implementation (called Spongy Castle for Android).
        // The following line defines Spongy Castle as the default security provider.
        // Alternatively, you can explicitly refer to the Spongy Castle provider using acronym
        // "SC" in the Java crypto API.
        Security.insertProviderAt(new org.spongycastle.jce.provider.BouncyCastleProvider(), 1);

        // Load native library ECDH-Curve25519-Android implementing Diffie-Hellman key
        // exchange with Elliptic Curve 25519.
        try {
            System.loadLibrary("ecdhcurve25519");
            Log.i(TAG, "Loaded ecdhcurve25519 library.");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Error loading ecdhcurve25519 library: " + e.getMessage());
        }
    }

    // Length of a nonce value in bytes.
    public static final int NONCE_LENGTH = 16;
    // Length of an HMAC. We use HMAC512-256 (512 bit SHA-512 truncated to lower 256 bit).
    public static final int HMAC_LENGTH = 32;
    // Length of shared secret. We use Elliptic Curve Diffie Hellman with Curve 25519,
    // which has a shared secret length of 32 bytes.
    public static final int SECRET_LENGTH = 32;
    // This key number indicates an invalid (unset) key.
    public static final int SECRET_INVALID = -1;

    public static final int REQUEST_ENABLE_BT = 1;
    public static final int REQUEST_SELECT_DEVICE = 2;

    // Standard base UUID (used to extend standard 16 bit UUIDs).
    public static final long STANDARD_BASE_UUID_MSB = 0x0000000000001000L;
    public static final long STANDARD_BASE_UUID_LSB = 0x800000805f9b34fbL;
    // 16 bit id of client characteristic configuration descriptor (CCCD).
    public static final short CCCD_ID = 0x2902;

    // Base UUID of Key20 Service and its characteristics:
    //
    //   0x0a9dXXXX-5ff4-4c58-8a53627de7cf1faf
    //
    // XXXX will be replaced by the 16 bit ID of the service and its characteristics defined below.
    public static final long BASE_UUID_MSB = 0x0a9d00005ff44c58L;
    public static final long BASE_UUID_LSB = 0x8a53627de7cf1fafL;

    // 16 bit ids of Key20 service and its characteristics
    public static final short KEY20_SERVICE_ID = 0x0001;
    public static final short CHARACTERISTIC_ID_NONCE = 0x0002;
    public static final short CHARACTERISTIC_ID_UNLOCK = 0x0003;
    public static final short CHARACTERISTIC_ID_CFG_IN = 0x0004;
    public static final short CHARACTERISTIC_ID_CFG_OUT = 0x0005;

    // Maximum value lengths of characteristics.
    public static final int MAX_CHAR_NONCE_LENGTH = 16;
    public static final int MAX_CHAR_UNLOCK_LENGTH = 18;
    public static final int MAX_CHAR_CFG_IN_LENGTH = 18;
    public static final int MAX_CHAR_CFG_OUT_LENGTH = 18;

    static private final String BUNDLE_UNLOCK_FRAGMENT = "fragment_unlock";
    static private final String BUNDLE_KEYEXCHANGE_FRAGMENT = "fragment_keyexchange";

    static private final String PREF_BLUETOOTH_ADDRESS = "bluetooth_address";
    static private final String PREF_KEYNO = "keyno";
    static private final String PREF_KEY = "key";

    static private final UUID serviceUUID = getUUID(BASE_UUID_MSB, BASE_UUID_LSB, KEY20_SERVICE_ID);
    static private final UUID nonceCharacteristicUUID =
            getUUID(BASE_UUID_MSB, BASE_UUID_LSB, CHARACTERISTIC_ID_NONCE);
    static private final UUID unlockCharacteristicUUID =
            getUUID(BASE_UUID_MSB, BASE_UUID_LSB, CHARACTERISTIC_ID_UNLOCK);
    static private final UUID cfgInCharacteristicUUID =
            getUUID(BASE_UUID_MSB, BASE_UUID_LSB, CHARACTERISTIC_ID_CFG_IN);
    static private final UUID cfgOutCharacteristicUUID =
            getUUID(BASE_UUID_MSB, BASE_UUID_LSB, CHARACTERISTIC_ID_CFG_OUT);
    static private final UUID cccdUUID =
            getUUID(STANDARD_BASE_UUID_MSB, STANDARD_BASE_UUID_LSB,
                    CCCD_ID);

    // Application states.
    private enum ApplicationStates {
        idle, unlock_waitBtEnabled, unlock_waitBtDevSelected, unlock_waitGattConnection,
        unlock_waitGattServicesDiscovered, unlock_waitNonceCCCDWriteAck, unlock_waitNonce,
        unlock_waitHMACPartOneAck, unlock_waitHMACPartTwoAck, keyex_waitBtEnabled,
        keyex_waitBtDevSelected, keyex_waitGattConnection, keyex_waitGattServicesDiscovered,
        keyex_waitCfgoutCCCDWriteAck, keyex_waitKeyPartOneAck, keyex_waitKeyPartTwoAck,
        keyex_waitServerKeyPartOne, keyex_waitServerKeyPartTwo, keyex_waitUserDecision
    }

    // This handler receives all application events relevant to trigger a transition
    // of the application state.
    public ApplicationEventHandler applicationEventHandler = null;

    // The current state of the application's state machine.
    private ApplicationStates appState;

    private BluetoothAdapter bluetoothAdapter = null;
    private BluetoothDevice bluetoothDevice = null;

    private GattCallbackHandler gattHandler = null;
    private BluetoothGatt gatt = null;
    private BluetoothGattService gattService = null;
    private BluetoothGattCharacteristic nonceCharacteristic = null;
    private BluetoothGattCharacteristic unlockCharacteristic = null;
    // Note that the cfgIn characteristic is for messages from client to server. So from
    // the point of view of the app, it's an outgoing value.
    private BluetoothGattCharacteristic cfginCharacteristic = null;
    // Note that the cfgOut characteristic is for messages from server to client. So from
    // the point of view of the app, it's an incoming value.
    private BluetoothGattCharacteristic cfgoutCharacteristic = null;
    private BluetoothGattDescriptor nonceCCCD = null;
    private BluetoothGattDescriptor cfgoutCCCD = null;

    private UnlockFragment fragmentUnlock = null;
    private KeyExchangeFragment fragmentKeyExchange = null;

    // The key information used for authenticating door unlock requests
    // (key number and shared secret).
    private int keyno;
    private byte[] secret;

    // Temporary security information used during key exchange or authentication.
    private byte[] hmac;
    private byte[] nonce;
    private byte[] keyexPublicServerKey;
    private byte[] keyexSecretClientKey;
    private byte[] keyexPublicClientKey;
    private byte[] keyexSharedSecret;
    int keyexKeyNo;

    /**
     * Handler for application-level events.
     *
     * A state machine is used to model the behavior of the application. The application
     * event handler receives all events relevant to trigger a transition of the application
     * state.
     *
     * The handler runs in the UI thread of the application. Any long-running action should
     * be offloaded to other threads.
     *
     * We make this class a static nested class, so a reference to the message handler (and
     * pending messages obtained from this handler) will not prevent the outer class from being
     * garbage-collected through a strong reference from a (non-static) inner to its outer class.
     * There is no point in keeping the activity alive just to receive pending messages, which are
     * anyway not handled anymore by a "dead" activity.
     */
    public static class ApplicationEventHandler extends Handler {

        // Event types of events triggering application state transitions.
        static public final int FAILURE = 0;
        static public final int START_DOOR_UNLOCK = 1;
        static public final int START_KEYEXCHANGE = 2;
        static public final int BT_DEVICE_SELECTED = 3;
        static public final int BT_ENABLED = 4;
        static public final int GATT_CONNECTED = 5;
        static public final int GATT_DISCONNECTED = 6;
        static public final int GATT_SERVICES_DISCOVERED = 7;
        static public final int GATT_CCCD_WRITE_ACK_NONCE = 8;
        static public final int GATT_CCCD_WRITE_ACK_CFGOUT = 9;
        static public final int GATT_NOTIFICATION_RCVD_NONCE = 10;
        static public final int GATT_NOTIFICATION_RCVD_CFGOUT = 11;
        static public final int GATT_WRITE_ACK_UNLOCK = 12;
        static public final int GATT_WRITE_ACK_CFGIN = 13;
        static public final int USER_KEY_CONFIRMED = 14;
        static public final int USER_KEY_DENIED = 15;

        private final WeakReference<MainActivity> activity;

        public ApplicationEventHandler(MainActivity activity) {
            this.activity = new WeakReference<MainActivity>(activity);
        }

        private void finishUnlockTask(MainActivity activity, int msgId) {
            Message msg;

            Toast.makeText(activity, msgId, Toast.LENGTH_LONG).show();
            activity.appState = ApplicationStates.idle;
            msg = activity.fragmentUnlock.msgHandler.obtainMessage(
                UnlockFragment.MessageHandler.TASK_FINISHED);
            activity.fragmentUnlock.msgHandler.sendMessage(msg);
        }

        private void finishKeyExchangeTask(MainActivity activity, int msgId) {
            Message msg;

            Toast.makeText(activity, msgId, Toast.LENGTH_LONG).show();
            activity.appState = ApplicationStates.idle;
            msg = activity.fragmentKeyExchange.msgHandler.obtainMessage(
                    KeyExchangeFragment.MessageHandler.TASK_FINISHED);
            activity.fragmentKeyExchange.msgHandler.sendMessage(msg);
        }

        @Override
        public void handleMessage(Message message) {
            // Get a strong reference to the activity.
            MainActivity activity = this.activity.get();
            if (activity == null) {
                // The activity has already been garbage-collected.
                return;
            }

            int event = message.what;
            Message outMsg;

            if ( (event == START_KEYEXCHANGE || event == START_DOOR_UNLOCK) &&
                    activity.appState != ApplicationStates.idle) {
                Toast.makeText(activity, R.string.err_task_in_progress,
                                Toast.LENGTH_LONG).show();
                return;
            }

            switch (activity.appState) {
                case idle:
                    if (event == START_DOOR_UNLOCK) {
                        if (activity.keyno == SECRET_INVALID) {
                            Toast.makeText(activity, R.string.err_no_key_defined,
                                Toast.LENGTH_LONG).show();
                            activity.appState = ApplicationStates.idle;
                        } else {
                            outMsg = activity.fragmentUnlock.msgHandler.obtainMessage(
                                    UnlockFragment.MessageHandler.TASK_STARTED);
                            activity.fragmentUnlock.msgHandler.sendMessage(outMsg);
                            if (activity.bluetoothAdapter.isEnabled()) {
                                outMsg = activity.applicationEventHandler.obtainMessage(BT_ENABLED);
                                activity.applicationEventHandler.sendMessage(outMsg);
                                activity.appState = ApplicationStates.unlock_waitBtEnabled;
                            } else {
                                Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
                                activity.startActivityForResult(intent, REQUEST_ENABLE_BT);
                                activity.appState = ApplicationStates.unlock_waitBtEnabled;
                            }
                        }
                    } else if (event == START_KEYEXCHANGE) {
                        activity.keyexSharedSecret = null;
                        activity.keyexKeyNo = message.arg1;
                        outMsg = activity.fragmentKeyExchange.msgHandler.obtainMessage(
                                KeyExchangeFragment.MessageHandler.TASK_STARTED);
                        activity.fragmentKeyExchange.msgHandler.sendMessage(outMsg);
                        if (activity.bluetoothAdapter.isEnabled()) {
                            outMsg = activity.applicationEventHandler.obtainMessage(BT_ENABLED);
                            activity.applicationEventHandler.sendMessage(outMsg);
                            activity.appState = ApplicationStates.keyex_waitBtEnabled;
                        } else {
                            Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
                            activity.startActivityForResult(intent, REQUEST_ENABLE_BT);
                            activity.appState = ApplicationStates.keyex_waitBtEnabled;
                        }
                    } else if (event == USER_KEY_CONFIRMED) {
                        if (activity.keyexSharedSecret == null) {
                            Toast.makeText(activity, R.string.err_no_sharedsecret,
                                    Toast.LENGTH_LONG).show();
                        } else {
                            Toast.makeText(activity, R.string.key_confirmed,
                                    Toast.LENGTH_LONG).show();
                            activity.setNewKey(activity.keyexKeyNo, activity.keyexSharedSecret);
                        }
                    }
                    break;
                case unlock_waitBtEnabled:
                    if (event == FAILURE) {
                        finishUnlockTask(activity, R.string.err_bluetooth_notavailable);
                    } else if (event == BT_ENABLED) {
                        if (activity.bluetoothDevice == null) {
                            activity.selectBtDevice();
                        } else {
                            outMsg = activity.applicationEventHandler.obtainMessage(
                                    BT_DEVICE_SELECTED);
                            activity.applicationEventHandler.sendMessage(outMsg);
                        }
                        activity.appState = ApplicationStates.unlock_waitBtDevSelected;
                    }
                    break;
                case keyex_waitBtEnabled:
                    if (event == FAILURE) {
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_notavailable);
                    } else if (event == BT_ENABLED) {
                        if (activity.bluetoothDevice == null) {
                            activity.selectBtDevice();
                        } else {
                            outMsg = activity.applicationEventHandler.obtainMessage(
                                    BT_DEVICE_SELECTED);
                            activity.applicationEventHandler.sendMessage(outMsg);
                        }
                        activity.appState = ApplicationStates.keyex_waitBtDevSelected;
                    }
                    break;
                case unlock_waitBtDevSelected:
                    if (event == FAILURE) {
                        Toast.makeText(activity, R.string.err_bluetooth_nodevice,
                                Toast.LENGTH_LONG).show();
                        activity.appState = ApplicationStates.idle;
                        outMsg = activity.fragmentUnlock.msgHandler.obtainMessage(
                                UnlockFragment.MessageHandler.TASK_FINISHED);
                        activity.fragmentUnlock.msgHandler.sendMessage(outMsg);
                    } else if (event == BT_DEVICE_SELECTED) {
                        activity.bluetoothDevice.connectGatt(activity, false,
                                activity.gattHandler);
                        activity.appState = ApplicationStates.unlock_waitGattConnection;
                    }
                    break;
                case keyex_waitBtDevSelected:
                    if (event == FAILURE) {
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_nodevice);
                    } else if (event == BT_DEVICE_SELECTED) {
                        activity.bluetoothDevice.connectGatt(activity, false,
                                activity.gattHandler);
                        activity.appState = ApplicationStates.keyex_waitGattConnection;
                    }
                    break;
                case unlock_waitGattConnection:
                    if (event == FAILURE) {
                        finishUnlockTask(activity, R.string.err_blueooth_connection);
                    } else if (event == GATT_CONNECTED) {
                        if (!activity.gatt.discoverServices()) {
                            activity.gattDone();
                            finishUnlockTask(activity, R.string.err_blueooth_connection);
                        } else {
                            activity.appState = ApplicationStates.unlock_waitGattServicesDiscovered;
                        }
                    }
                    break;
                case keyex_waitGattConnection:
                    if (event == FAILURE) {
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_connection);
                    } else if (event == GATT_CONNECTED) {
                        if (!activity.gatt.discoverServices()) {
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_connection);
                        } else {
                            activity.appState = ApplicationStates.keyex_waitGattServicesDiscovered;
                        }
                    }
                    break;
                case unlock_waitGattServicesDiscovered:
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        finishUnlockTask(activity, R.string.err_bluetooth_discovery);
                    } else if (event == GATT_SERVICES_DISCOVERED) {
                        activity.gattService = activity.gatt.getService(serviceUUID);
                        if (activity.gattService == null) {
                            activity.gattDone();
                            finishUnlockTask(activity, R.string.err_bluetooth_service);
                            return;
                        }
                        // GATT Service is available.
                        if (!activity.getUnlockCharacteristics()) {
                            activity.gattDone();
                            finishUnlockTask(activity, R.string.err_bluetooth_characteristic);
                            return;
                        }
                        // Required characteristics are available.
                        if (!activity.subscribeNonceCharateristic()) {
                            activity.gattDone();
                            finishUnlockTask(activity, R.string.err_bluetooth_notification);
                            return;
                        }
                        // Waiting for the ack. for the CCCD write operation before waiting for
                        // the indication leads to race conditions. Sometimes, the CCCD write ack.
                        // arrives *after* the indication. Since the arrival of an
                        // indication is an implicit ack. for the CCCD write operation, we can
                        // safely skip the state "unlock_waitNonceCCCDWriteAck" to avoid such race
                        // conditions.
                        //activity.appState = ApplicationStates.unlock_waitNonceCCCDWriteAck;
                        activity.appState = ApplicationStates.unlock_waitNonce;
                    }
                    break;
                case keyex_waitGattServicesDiscovered:
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_discovery);
                    } else if (event == GATT_SERVICES_DISCOVERED) {
                        activity.gattService = activity.gatt.getService(serviceUUID);
                        if (activity.gattService == null) {
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_service);
                            return;
                        }
                        // GATT Service is available.
                        if (!activity.getKeyExCharacteristics()) {
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_characteristic);
                            return;
                        }
                        // Required characteristics are available.
                        if (!activity.subscribeCfgoutCharateristic()) {
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_notification);
                            return;
                        }
                        activity.appState = ApplicationStates.keyex_waitCfgoutCCCDWriteAck;
                    }
                    break;
                case unlock_waitNonceCCCDWriteAck:
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishUnlockTask(activity, R.string.err_bluetooth_notification);
                    } else if (event == GATT_CCCD_WRITE_ACK_NONCE) {
                        // We are now ready to communicate with the service.
                        // Next, we run the protocol for unlocking the door.
                        activity.appState = ApplicationStates.unlock_waitNonce;
                    }
                    break;
                case keyex_waitCfgoutCCCDWriteAck:
                    if (event == FAILURE  || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_notification);
                    } else if (event == GATT_CCCD_WRITE_ACK_CFGOUT) {
                        // We are now ready to communicate with the service.
                        // Next, we run the protocol for unlocking the door.
                        activity.calculateClientKeyPair();
                        if (!activity.sendPublicClientKey(0)) {
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_write);
                        } else {
                            activity.appState = ApplicationStates.keyex_waitKeyPartOneAck;
                        }
                    }
                    break;
                case keyex_waitKeyPartOneAck:
                    if (event == FAILURE  || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_write);
                    } else if (event == GATT_WRITE_ACK_CFGIN) {
                        if (!activity.sendPublicClientKey(1)) {
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_write);
                        } else {
                            activity.appState = ApplicationStates.keyex_waitKeyPartTwoAck;
                        }
                    }
                    break;
                case keyex_waitKeyPartTwoAck:
                    if (event == FAILURE  || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_write);
                    } else if (event == GATT_WRITE_ACK_CFGIN) {
                        activity.appState = ApplicationStates.keyex_waitServerKeyPartOne;
                    }
                    break;
                case keyex_waitServerKeyPartOne:
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_notification);
                    } else if (event == GATT_NOTIFICATION_RCVD_CFGOUT) {
                        byte[] value = (byte[]) message.obj;
                        if (value[0] != activity.keyexKeyNo || value[1] != 0) {
                            // Server sent wrong key part or key number.
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_notification);
                        } else {
                            activity.keyexPublicServerKey = new byte[SECRET_LENGTH];
                            System.arraycopy(value, 2, activity.keyexPublicServerKey, 0, 16);
                            activity.appState = ApplicationStates.keyex_waitServerKeyPartTwo;
                        }
                    }
                    break;
                case keyex_waitServerKeyPartTwo:
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishKeyExchangeTask(activity, R.string.err_bluetooth_notification);
                    } else if (event == GATT_NOTIFICATION_RCVD_CFGOUT) {
                        byte[] value = (byte[]) message.obj;
                        if (value[0] != activity.keyexKeyNo || value[1] != 1) {
                            // Server sent wrong key part or key number.
                            activity.gattDone();
                            finishKeyExchangeTask(activity, R.string.err_bluetooth_notification);
                        } else {
                            System.arraycopy(value, 2, activity.keyexPublicServerKey, 16, 16);
                            // We have received all data from the server that we need. Now
                            // calculate the shared secret and let the user decide whether
                            // it is valid.
                            activity.gattDone();
                            // If calculating the shared secret should take too long, move it from
                            // the UI thread to another thread.
                            activity.calculateSharedSecret();
                            activity.displaySecretChecksum();
                            finishKeyExchangeTask(activity, R.string.keyex_finished);
                        }
                    }
                    break;
                case unlock_waitNonce:
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishUnlockTask(activity, R.string.err_bluetooth_notification);
                    } else if (event == GATT_NOTIFICATION_RCVD_NONCE) {
                        activity.nonce = (byte []) message.obj;
                        // Calculating a SHA-512 hash should be very fast, so we can do it
                        // on the UI thread.
                        activity.hmac = activity.calculateHMAC512_256(activity.nonce,
                                activity.secret);
                        if (!activity.sendHMAC(activity.hmac, 0)) {
                            activity.gattDone();
                            finishUnlockTask(activity, R.string.err_bluetooth_write);
                        } else {
                            activity.appState = ApplicationStates.unlock_waitHMACPartOneAck;
                        }
                    }
                    break;
                case unlock_waitHMACPartOneAck:
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        activity.gattDone();
                        finishUnlockTask(activity, R.string.err_bluetooth_write);
                    } else if (event == GATT_WRITE_ACK_UNLOCK) {
                        if (!activity.sendHMAC(activity.hmac, 1)) {
                            activity.gattDone();
                            finishUnlockTask(activity, R.string.err_bluetooth_write);
                        } else {
                            activity.appState = ApplicationStates.unlock_waitHMACPartTwoAck;
                        }
                    }
                    break;
                case unlock_waitHMACPartTwoAck:
                    activity.gattDone();
                    if (event == FAILURE || event == GATT_DISCONNECTED) {
                        finishUnlockTask(activity, R.string.err_bluetooth_write);
                    } else if (event == GATT_WRITE_ACK_UNLOCK) {
                        finishUnlockTask(activity, R.string.door_open_finished);
                        activity.gattDone();
                    }
                    break;
            }
        }
    }

    private void gattDone() {
        gattService = null;
        nonceCharacteristic = null;
        unlockCharacteristic = null;
        cfginCharacteristic = null;
        cfgoutCharacteristic = null;
        if (gatt != null) {
            gatt.close();
            gatt = null;
        }
    }

    private boolean subscribeNonceCharateristic() {
        // Subscribe to Nonce characteristic value updates.

        // The following call prepares the subscriber (the Android app) to receive indications.
        if (!gatt.setCharacteristicNotification(nonceCharacteristic, true)) {
            return false;
        }

        // Indications are enabled on the remote device by writing a flag to the
        // CCCD (Client Characteristic Configuration Descriptor).
        nonceCCCD = nonceCharacteristic.getDescriptor(cccdUUID);
        if (nonceCCCD == null) {
            return false;
        }
        if (!nonceCCCD.setValue(BluetoothGattDescriptor.ENABLE_INDICATION_VALUE)) {
            return false;
        }
        if (!gatt.writeDescriptor(nonceCCCD)) {
            return false;
        }

        return true;
    }

    private boolean subscribeCfgoutCharateristic() {
        // Subscribe to Nonce characteristic value updates.

        // The following call prepares the subscriber (the Android app) to receive indications.
        if (!gatt.setCharacteristicNotification(cfgoutCharacteristic, true)) {
            return false;
        }

        // Indications are enabled on the remote device by writing a flag to the
        // CCCD (Client Characteristic Configuration Descriptor).
        cfgoutCCCD = cfgoutCharacteristic.getDescriptor(cccdUUID);
        if (cfgoutCCCD == null) {
            return false;
        }
        if (!cfgoutCCCD.setValue(BluetoothGattDescriptor.ENABLE_INDICATION_VALUE)) {
            return false;
        }
        if (!gatt.writeDescriptor(cfgoutCCCD)) {
            return false;
        }

        return true;
    }

    private boolean getUnlockCharacteristics() {
        nonceCharacteristic = gattService.getCharacteristic(nonceCharacteristicUUID);
        if (nonceCharacteristic == null) {
            return false;
        }

        unlockCharacteristic = gattService.getCharacteristic(unlockCharacteristicUUID);
        if (unlockCharacteristic == null) {
            return false;
        }

        return true;
    }

    private boolean getKeyExCharacteristics() {
        cfgoutCharacteristic = gattService.getCharacteristic(cfgOutCharacteristicUUID);
        if (cfgoutCharacteristic == null) {
            return false;
        }

        cfginCharacteristic = gattService.getCharacteristic(cfgInCharacteristicUUID);
        if (cfginCharacteristic == null) {
            return false;
        }

        return true;
    }

    /**
     * Adapter to integrated fragments into the tabs of the tab view.
     */
    public class FPAdapter extends FragmentPagerAdapter {
        private final int PAGE_COUNT = 2;

        public FPAdapter(FragmentManager fm) {
            super(fm);
        }

        @Override
        public int getCount() {
            return PAGE_COUNT;
        }

        @Override
        public Fragment getItem(int position) {
            Fragment fragment;
            switch (position) {
                case 0 :
                    fragmentUnlock = UnlockFragment.newInstance();
                    fragment = fragmentUnlock;
                    break;
                case 1:
                    fragmentKeyExchange = KeyExchangeFragment.newInstance();
                    fragment = fragmentKeyExchange;
                    break;
                default:
                    fragment = null;
            }

            return fragment;
        }

        @Override
        public CharSequence getPageTitle(int position) {
            // Generate tab labels based on tab position
            String tabLabel = null;

            switch (position) {
                case 0:
                    tabLabel = getResources().getString(R.string.tab_label_unlock);
                    break;
                case 1:
                    tabLabel = getResources().getString(R.string.tab_label_keyexchange);
                    break;
            }

            return tabLabel;
        }
    }

    /**
     * Handler for GATT callbacks.
     */
    private class GattCallbackHandler extends BluetoothGattCallback {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            Message msg;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(TAG, "GATT: connection failure.");
                msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                applicationEventHandler.sendMessage(msg);
            } else if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i(TAG, "GATT: connected.");
                MainActivity.this.gatt = gatt;
                msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.GATT_CONNECTED);
                applicationEventHandler.sendMessage(msg);
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.i(TAG, "GATT: disconnected.");
                msg = applicationEventHandler.obtainMessage(
                        ApplicationEventHandler.GATT_DISCONNECTED);
                applicationEventHandler.sendMessage(msg);
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            Message msg;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(TAG, "GATT: service discovery failed.");
                msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                applicationEventHandler.sendMessage(msg);
            } else {
                Log.i(TAG, "GATT: services discovered.");
                msg = applicationEventHandler.obtainMessage(
                        ApplicationEventHandler.GATT_SERVICES_DISCOVERED);
                applicationEventHandler.sendMessage(msg);
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt,
                                            BluetoothGattCharacteristic characteristic) {
            Message msg;
            byte[] value;
            if (characteristic == nonceCharacteristic) {
                Log.i(TAG, "GATT: notification Nonce char.");
                value = characteristic.getValue();
                if (value.length != MAX_CHAR_NONCE_LENGTH) {
                    Log.e(TAG, "GATT: Nonce char. has invalid length");
                    msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                    applicationEventHandler.sendMessage(msg);
                } else {
                    msg = applicationEventHandler.obtainMessage(
                            ApplicationEventHandler.GATT_NOTIFICATION_RCVD_NONCE);
                    msg.obj = value;
                    applicationEventHandler.sendMessage(msg);
                }
            } else if (characteristic == cfgoutCharacteristic) {
                Log.i(TAG, "GATT: notification CfgOut char.");
                value = characteristic.getValue();
                if (value.length != MAX_CHAR_CFG_OUT_LENGTH) {
                    Log.e(TAG, "GATT: CfgOut char. has invalid length.");
                    msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                    applicationEventHandler.sendMessage(msg);
                } else {
                    msg = applicationEventHandler.obtainMessage(
                            ApplicationEventHandler.GATT_NOTIFICATION_RCVD_CFGOUT);
                    msg.obj = value;
                    applicationEventHandler.sendMessage(msg);
                }
            }
        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt,
                                    BluetoothGattCharacteristic characteristic,
                                    int status) {
            Message msg;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.i(TAG, "GATT: write failure.");
                msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                applicationEventHandler.sendMessage(msg);
            } else {
                if (characteristic == unlockCharacteristic) {
                    Log.i(TAG, "GATT: write ack for Unlock char.");
                    msg = applicationEventHandler.obtainMessage(
                            ApplicationEventHandler.GATT_WRITE_ACK_UNLOCK);
                    applicationEventHandler.sendMessage(msg);
                } else if (characteristic == cfginCharacteristic) {
                    Log.i(TAG, "GATT: write ack for CfgIn char.");
                    msg = applicationEventHandler.obtainMessage(
                            ApplicationEventHandler.GATT_WRITE_ACK_CFGIN);
                    applicationEventHandler.sendMessage(msg);
                }
            }
        }

        @Override
        public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor,
                int status) {
            Message msg;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(TAG, "GATT: CCCD write failure.");
                msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                applicationEventHandler.sendMessage(msg);
            } else {
                if (descriptor == nonceCCCD) {
                    Log.i(TAG, "GATT: wrote Nonce CCCD.");
                        msg = applicationEventHandler.obtainMessage(
                                ApplicationEventHandler.GATT_CCCD_WRITE_ACK_NONCE);
                        applicationEventHandler.sendMessage(msg);
                } else if (descriptor == cfgoutCCCD) {
                    Log.i(TAG, "GATT: wrote CfgOut CCCD.");
                    msg = applicationEventHandler.obtainMessage(
                            ApplicationEventHandler.GATT_CCCD_WRITE_ACK_CFGOUT);
                    applicationEventHandler.sendMessage(msg);
                }
            }
        }
    }

    private void displaySecretChecksum() {
        // As checksum, we use a SHA512 hash of the shared secret
        // truncated to the lower 8 bytes (displayed as 16 hex digits).
        byte[] hash = calculateSHA512(keyexSharedSecret);
        byte[] checksum = new byte[8];
        System.arraycopy(hash, 0, checksum, 0, 8);

        Message msg = fragmentKeyExchange.msgHandler.obtainMessage(
                KeyExchangeFragment.MessageHandler.DISPLAY_SECRET_CHECKSUM);
        msg.obj = checksum;
        fragmentKeyExchange.msgHandler.sendMessage(msg);
    }

    private void setNewKey(int keyno, byte[] secret) {
        this.keyno = keyno;
        this.secret = secret;

        // Store key in preferences.
        // Key value (secret) is stored as Base-64-encoded string.
        String keybase64 = Base64.encodeToString (secret, Base64.DEFAULT);
        SharedPreferences prefs = getPreferences(Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(PREF_KEYNO, keyno);
        editor.putString(PREF_KEY, keybase64);
        editor.commit();
    }

    private byte[] calculateSHA512(byte[] data) {
        byte[] hash;

        try {
            MessageDigest md = MessageDigest.getInstance("SHA-512");
            hash = md.digest(data);
        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG, "Cannot find SHA-512 algorithm: " + e.getMessage());
            return null;
        }

        return hash;
    }

    private byte[] calculateHMAC512_256(byte[] data, byte[] key) {
        byte[] hmac512;
        byte[] hmac512_256;

        try {
            Mac mac = null;
            SecretKeySpec keySpec = new SecretKeySpec(key, "HMac-SHA512");
            mac = Mac.getInstance("HMac-SHA512");
            mac.init(keySpec);
            hmac512 = mac.doFinal(data);
        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG, e.getMessage());
            return null;
        } catch (InvalidKeyException e) {
            Log.e(TAG, e.getMessage());
            return null;
        }

        // Truncate HMAC512 to 256 bits using the 32 lower-order bytes.
        hmac512_256 = new byte[HMAC_LENGTH];
        System.arraycopy(hmac512, 0, hmac512_256, 0, HMAC_LENGTH);

        return hmac512_256;
    }

    private void calculateClientKeyPair() {
        SecureRandom random = new SecureRandom();
        keyexSecretClientKey = ECDHCurve25519.generate_secret_key(random);
        keyexPublicClientKey = ECDHCurve25519.generate_public_key(keyexSecretClientKey);
    }

    private void calculateSharedSecret() {
        keyexSharedSecret = ECDHCurve25519.generate_shared_secret(keyexSecretClientKey,
                keyexPublicServerKey);
    }

    private boolean sendPublicClientKey(int part) {
        byte[] characteristicValue = new byte[MAX_CHAR_CFG_IN_LENGTH];
        characteristicValue[0] = (byte) keyexKeyNo;
        characteristicValue[1] = (byte) part;

        switch (part) {
            case 0:
                System.arraycopy(keyexPublicClientKey, 0, characteristicValue, 2, 16);
                break;
            case 1:
                System.arraycopy(keyexPublicClientKey, 16, characteristicValue, 2, 16);
                break;
            default:
                Log.e(TAG, "Invalid public key part.");
        }
        if (!cfginCharacteristic.setValue(characteristicValue))
            return false;

        cfginCharacteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);

        return (gatt.writeCharacteristic(cfginCharacteristic));
    }

    private boolean sendHMAC(byte[] hmac, int part) {
        byte[] characteristicValue = new byte[MAX_CHAR_UNLOCK_LENGTH];
        characteristicValue[0] = (byte) keyno;
        characteristicValue[1] = (byte) part;

        switch (part) {
            case 0:
                System.arraycopy(hmac, 0, characteristicValue, 2, 16);
                break;
            case 1:
                System.arraycopy(hmac, 16, characteristicValue, 2, 16);
                break;
            default:
                Log.e(TAG, "Invalid HMAC part.");
                return false;
        }
        if (!unlockCharacteristic.setValue(characteristicValue))
            return false;

        unlockCharacteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);

        return (gatt.writeCharacteristic(unlockCharacteristic));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        if (bluetoothAdapter == null) {
            BluetoothManager bluetoothManager = (BluetoothManager) getSystemService(
                    Context.BLUETOOTH_SERVICE);
            bluetoothAdapter = bluetoothManager.getAdapter();
        }

        String btAddr = null;
        SharedPreferences preferences = getPreferences(Context.MODE_PRIVATE);
        if (preferences.contains(PREF_BLUETOOTH_ADDRESS)) {
            btAddr = preferences.getString(PREF_BLUETOOTH_ADDRESS, "foo");
        }
        if (btAddr != null) {
            bluetoothDevice = bluetoothAdapter.getRemoteDevice(btAddr);
        }

        gattHandler = new GattCallbackHandler();

        if (preferences.contains(PREF_KEYNO) && preferences.contains(PREF_KEY)) {
            preferences.getInt(PREF_KEYNO, 0);
            String keybase64 = preferences.getString(PREF_KEY, "foo");
            secret = Base64.decode(keybase64, Base64.DEFAULT);
        } else {
            keyno = SECRET_INVALID;
        }


        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        String appName = getString(R.string.app_name);
        toolbar.setTitle(appName);
        setSupportActionBar(toolbar);

        FragmentManager fm = getSupportFragmentManager();
        ViewPager viewPager = (ViewPager) findViewById(R.id.viewpager);
        FPAdapter fpAdapter = new FPAdapter(fm);
        viewPager.setAdapter(fpAdapter);

        if (savedInstanceState != null) {
            // Get references to already existing fragments.
            if (savedInstanceState.containsKey(BUNDLE_UNLOCK_FRAGMENT)) {
                String tag = savedInstanceState.getString(BUNDLE_UNLOCK_FRAGMENT);
                fragmentUnlock = (UnlockFragment) fm.findFragmentByTag(tag);
            }
            if (savedInstanceState.containsKey(BUNDLE_KEYEXCHANGE_FRAGMENT)) {
                String tag = savedInstanceState.getString(BUNDLE_KEYEXCHANGE_FRAGMENT);
                fragmentKeyExchange = (KeyExchangeFragment) fm.findFragmentByTag(tag);
            }
        }

        TabLayout tabLayout = (TabLayout) findViewById(R.id.tablayout);
        tabLayout.setupWithViewPager(viewPager);

        applicationEventHandler = new ApplicationEventHandler(this);
        appState = ApplicationStates.idle;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    /**
     * Trigger selection of Bluetooth device in dedicated activity.
     */
    private void selectBtDevice() {
        Intent intent = new Intent(this, DeviceSelectionActivity.class);
        startActivityForResult(intent, REQUEST_SELECT_DEVICE);
    }

    /**
     * Creates a 128 bit UUID of a service or characteristic from a 128 base UUID and 16 bit
     * service/characteristic id.
     *
     * Example: Given
     * - 128 bit base UUID 550eXXXX-e29b-11d4-a716-446655440000
     * - 16 bit service ID: 0x1234
     * The resulting UUID is generated by replacing XXXX by the 16 bit id of the service:
     * - UUID: 550e1234-e29b-11d4-a716-446655440000
     *
     * @param baseMSB most significant bits of the base UUID
     * @param baseLSB least significant bits of the base UUID
     * @param id 16 bit id of the service or characteristic
     * @return UUID of the service of characteristic
     */
    public static UUID getUUID(long baseMSB, long baseLSB, short id) {
        long msb = baseMSB & 0xffff0000ffffffffL;
        msb |= ((long) id)<<32;

        return new UUID(msb, baseLSB);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        //toolbar.inflateMenu(R.menu.menu_main);
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        if (id == R.id.action_scan) {
            selectBtDevice();
            return true;
        }

        if (id == R.id.action_about) {
            showAboutDialog();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Message msg;
        switch (requestCode) {
            case REQUEST_ENABLE_BT:
                if (resultCode == RESULT_OK) {
                    // Bluetooth has been enabled.
                    msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.BT_ENABLED);
                    applicationEventHandler.sendMessage(msg);
                } else {
                    // No Bluetooth available.
                    msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                    applicationEventHandler.sendMessage(msg);
                }
                break;
            case REQUEST_SELECT_DEVICE:
                if (resultCode == RESULT_OK) {
                    // Bluetooth device has been selected.
                    bluetoothDevice = data.getParcelableExtra(
                            DeviceSelectionActivity.RESULT_BLUETOOTHDEVICE);

                    // Remember this device in persistent preferences.
                    SharedPreferences prefs = getPreferences(Context.MODE_PRIVATE);
                    SharedPreferences.Editor editor = prefs.edit();
                    editor.putString(PREF_BLUETOOTH_ADDRESS, bluetoothDevice.getAddress());
                    editor.commit();

                    msg = applicationEventHandler.obtainMessage(
                            ApplicationEventHandler.BT_DEVICE_SELECTED);
                    applicationEventHandler.sendMessage(msg);
                } else {
                    msg = applicationEventHandler.obtainMessage(ApplicationEventHandler.FAILURE);
                    applicationEventHandler.sendMessage(msg);
                }
                break;
        }
    }

    private void showAboutDialog() {
        View view = getLayoutInflater().inflate(R.layout.dialog_about, null, false);
        TextView textView = (TextView) view.findViewById(R.id.about_message);
        textView.setText(Html.fromHtml(getString(R.string.about_message)));
        textView.setMovementMethod(LinkMovementMethod.getInstance());

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setIcon(R.mipmap.ic_launcher);
        builder.setTitle(R.string.app_name);
        builder.setView(view);
        builder.create();
        builder.show();
    }

    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        super.onSaveInstanceState(savedInstanceState);

        // We do not save information about the Bluetooth device or key since this
        // information is recovered from preferences. In particular for the key,
        // we do not want so store private information at too many places, since
        // this increases the attack surface for leaking keys.

        // Save tags of existing fragments, so we can retrieve fragment references later in the
        // newly created activity via the fragments manager.
        if (fragmentUnlock != null) {
            String tag = fragmentUnlock.getTag();
            savedInstanceState.putString(BUNDLE_UNLOCK_FRAGMENT, tag);
        }
        if (fragmentKeyExchange != null) {
            String tag = fragmentKeyExchange.getTag();
            savedInstanceState.putString(BUNDLE_KEYEXCHANGE_FRAGMENT, tag);
        }
    }
}
