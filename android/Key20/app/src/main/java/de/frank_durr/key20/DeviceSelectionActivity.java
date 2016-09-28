/**
 * This file is part of BLE-V-Monitor.
 *
 * Copyright 2015 Frank Duerr
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

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.design.widget.FloatingActionButton;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.util.LinkedList;
import java.util.List;

/**
 * Activity for selecting a Bluetooth device supporting the required Key20 GATT service.
 */
public class DeviceSelectionActivity extends android.support.v7.app.AppCompatActivity implements
        BluetoothAdapter.LeScanCallback {

    public final static String TAG = DeviceSelectionActivity.class.getName();

    public static final String RESULT_BLUETOOTHDEVICE = "BluetoothDevice";

    public static final int REQUEST_ENABLE_BT = 1;

    // Scan timeout in milliseconds.
    public static final int SCAN_TIMEOUT = 15000;

    public static final int SELECTED_NONE = -1;

    private static final int PERMISSION_REQUEST_COARSE_LOCATION = 1;

    private BluetoothAdapter bluetoothAdapter = null;

    private List<BLEDevice> devices;

    private Handler timeoutHandler;

    private boolean isScanning = false;

    private ListView deviceListView;
    private ArrayAdapter<BLEDevice> deviceListAdapter;
    private ItemClickListener itemClickListener;
    private TextView textviewScanstatus;

    private int selected;

    private class BLEDevice {
        public BluetoothDevice device;

        public BLEDevice(BluetoothDevice device) {
            this.device = device;
        }

        @Override
        public String toString() {
            if (device.getName() == null) {
                return "[" + device.getAddress() + "]";
            } else {
                return device.getName() + " [" + device.getAddress() + "]";
            }
        }

    }

    /**
     * Logic to stop Bluetooth scanning after timeout.
     */
    private class ScanTimeoutLogic implements Runnable {
        public ScanTimeoutLogic() {
        }

        @Override
        public void run() {
            DeviceSelectionActivity.this.stopScanning();
        }
    }

    private class ItemClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            selected = position;
            finished();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_device_selection);

        if (bluetoothAdapter == null) {
            BluetoothManager bluetoothManager = (BluetoothManager) getSystemService(
                    Context.BLUETOOTH_SERVICE);
            bluetoothAdapter = bluetoothManager.getAdapter();
        }

        Toolbar toolbar = (Toolbar) findViewById(R.id.ds_toolbar);
        String appName = getString(R.string.app_name);
        toolbar.setTitle(appName);
        setSupportActionBar(toolbar);

        selected = SELECTED_NONE;
        devices = new LinkedList<>();
        deviceListAdapter = new ArrayAdapter<>(this, R.layout.textview_deviceselection,
                devices);
        deviceListView = (ListView) findViewById(R.id.listView_deviceselection);
        deviceListView.setAdapter(deviceListAdapter);
        itemClickListener = new ItemClickListener();
        deviceListView.setOnItemClickListener(itemClickListener);

        FloatingActionButton fab = (FloatingActionButton) findViewById(R.id.fab_blescan);
        fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startScanning();
            }
        });

        textviewScanstatus = (TextView) findViewById(R.id.textViewScanStatus);
        textviewScanstatus.setText(getString(R.string.scanning));
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        //toolbar.inflateMenu(R.menu.menu_main);
        getMenuInflater().inflate(R.menu.menu_deviceselection, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        if (id == R.id.action_startscanning) {
            startScanning();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onStart() {
        super.onStart();

        startScanning();
    }

    @Override
    public void onStop() {
        super.onStop();

        stopScanning();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case REQUEST_ENABLE_BT :
                if (resultCode == RESULT_OK) {
                    // BLE has been enabled
                    startScanning();
                } else {
                    // Cannot scan for devices w/o Bluetooth -> return (without result)
                    finished();
                }
                break;
        }
    }

    @Override
    public void onLeScan(BluetoothDevice device, int rssi, byte[] scanRecord) {
        Log.i(TAG, "Discovered device: " + device.getName() + " " + device.getAddress());

        // Check, whether we have already seen this device
        for (BLEDevice d : devices) {
            if (d.device.hashCode() == device.hashCode()) {
                // This device, we know already
                return;
            }
        }

        // Device is new

        BLEDevice newDevice = new BLEDevice(device);
        devices.add(newDevice);
        deviceListAdapter.notifyDataSetChanged();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String permissions[],
                                           int[] grantResults) {
        switch (requestCode) {
        case PERMISSION_REQUEST_COARSE_LOCATION: {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.d(TAG, "Permission granted: coarse location");
                startScanning();
            } else {
                finished();
            }
        }
    }
}

    /**
     * Stop Bluetooth scan.
     */
    private void stopScanning() {
        Log.i(TAG, "scanning stopped");

        if (!isScanning){
            return;
        }

        textviewScanstatus.setText(getString(R.string.notscanning));

        bluetoothAdapter.stopLeScan(this);
        isScanning = false;
    }

    /**
     * Start Bluetooth scan.
     */
    private void startScanning() {
        Log.i(TAG, "scanning started");

        // In Android M, background Bluetooth scanning requires permission to coarse locations.
        // With the new permission system of Android M, we need to request permissions
        // at runtime.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Android M Permission check 
            if (checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION) !=
                    PackageManager.PERMISSION_GRANTED) {
                AlertDialog.Builder builder = new AlertDialog.Builder(this);
                builder.setTitle(getResources().getString(R.string.permission_dialog_label));
                builder.setMessage(getResources().getString(
                        R.string.permission_request_coarse_location));
                builder.setPositiveButton(android.R.string.ok, null);
                builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
                    public void onDismiss(DialogInterface dialog) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                            requestPermissions(
                                    new String[]{Manifest.permission.ACCESS_COARSE_LOCATION},
                                    PERMISSION_REQUEST_COARSE_LOCATION);
                        }
                    }
                });
                builder.show();
                return; // We come back later, when user has granted access.
            }
        }

        if (isScanning) {
            return;
        }

        if (!bluetoothAdapter.isEnabled()) {
            Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(intent, REQUEST_ENABLE_BT);
            return;
        }

        textviewScanstatus.setText(getString(R.string.scanning));

        devices.clear();
        deviceListAdapter.notifyDataSetChanged();

        isScanning = true;
        bluetoothAdapter.startLeScan(this);

        // Stop scanning after timeout.
        // The handler is running on the UI thread.
        timeoutHandler = new Handler(Looper.getMainLooper());
        ScanTimeoutLogic timeoutLogic = new ScanTimeoutLogic();
        timeoutHandler.postDelayed(timeoutLogic, SCAN_TIMEOUT);
    }

    /**
     * Finish the activity, with or without result.
     */
    private void finished() {
        stopScanning();

        if (selected == SELECTED_NONE) {
            // Nothing selected -> finish without result.
            Intent result = new Intent();
            setResult(Activity.RESULT_CANCELED, result);
        } else {
            // Device selected -> finish with result
            BluetoothDevice selectedDevice = devices.get(selected).device;
            Intent result = new Intent();
            result.putExtra(RESULT_BLUETOOTHDEVICE, selectedDevice);
            setResult(Activity.RESULT_OK, result);
        }

        finish();
    }
}
