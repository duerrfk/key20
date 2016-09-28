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

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.design.widget.FloatingActionButton;
import android.support.v4.app.Fragment;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * This fragment shows the UI for performing a key exchange with the remote lock control device.
 */
public class KeyExchangeFragment extends Fragment {

    private Spinner spinnerKeyNo;
    private Button buttonConfirmKey;
    private TextView textViewChecksum;
    private TextView textViewInstructions;
    private FloatingActionButton floatingActionButton;
    private ProgressBar progressBar;

    private WeakReference<MainActivity> activity;

    public MessageHandler msgHandler;

    /**
     * Handler for messages to the fragment.
     *
     * The handler runs in the UI thread.
     *
     * We make this class a static nested class, so a reference to the message handler (and
     * pending messages obtained from this handler) will not prevent the outer class from being
     * garbage-collected through a strong reference from a (non-static) inner to its outer class.
     * There is no point in keeping the fragment alive just to receive pending messages, which are
     * anyway not handled anymore by a "dead" fragment.
     */
    public static class MessageHandler extends Handler {
        public final static int DISPLAY_SECRET_CHECKSUM = 0;
        public final static int TASK_STARTED = 1;
        public final static int TASK_FINISHED = 2;

        private final WeakReference<KeyExchangeFragment> fragment;

        public MessageHandler(KeyExchangeFragment fragment) {
            this.fragment = new WeakReference<KeyExchangeFragment>(fragment);
        }

        @Override
        public void handleMessage(Message message) {
            // Get a strong reference to the fragment.
            KeyExchangeFragment f = fragment.get();
            if (f == null) {
                // The fragment has been garbage-collected.
                return;
            }

            switch (message.what) {
                case DISPLAY_SECRET_CHECKSUM :
                    // Checksum is displayed as 16 hex digits with the lowest-order byte
                    // at string index 0/1, i.e., checksum[0] is displayed leftmost.
                    byte[] checksum = (byte[]) message.obj;
                    String checksumStr = binarytoHexString(checksum);
                    f.textViewChecksum.setText(checksumStr);
                    break;
                case TASK_STARTED :
                    // Delete old checksum, so it will not be confused with the new one.
                    f.textViewChecksum.setText("");
                    f.progressBar.setVisibility(View.VISIBLE);
                    break;
                case TASK_FINISHED :
                    f.progressBar.setVisibility(View.INVISIBLE);
                    break;
            }
        }
    }

    static private String binarytoHexString(byte[] binary)
    {
        StringBuilder sb = new StringBuilder(binary.length*2);

        // Lowest order byte at index 0/1 of the string.
        for (int i = 0; i < binary.length; i++) {
            // High nibble first, i.e., to the left.
            // Note that bytes are signed in Java. However, "int x = abyte&0xff" will always
            // return an int value of x between 0 and 255.
            // "int v = binary[i]>>4" (without &0xff) does *not* work.
            int v = (binary[i]&0xff)>>4;
            char c;
            if (v < 10) {
                c = (char) ('0'+v);
            } else {
                c = (char) ('a'+v-10);
            }
            sb.append(c);
            // low nibble
            v = binary[i]&0x0f;
            if (v < 10) {
                c = (char) ('0'+v);
            } else {
                c = (char) ('a'+v-10);
            }
            sb.append(c);
        }

        return sb.toString();
    }

    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     *
     * @return A new instance of fragment UnlockFragment.
     */
    public static KeyExchangeFragment newInstance() {
        KeyExchangeFragment fragment = new KeyExchangeFragment();

        Bundle args = new Bundle();
        fragment.setArguments(args);

        return fragment;
    }

    /**
     * An empty constructor is required.
     */
    public KeyExchangeFragment() {
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        View v = inflater.inflate(R.layout.fragment_keyexchange, container, false);

        progressBar = (ProgressBar) v.findViewById(R.id.progressbar_keyexchange);
        progressBar.setVisibility(View.INVISIBLE);

        spinnerKeyNo = (Spinner) v.findViewById(R.id.spinner_keyno);
        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(v.getContext(),
                R.array.spinner_keyno_choices, android.R.layout.simple_spinner_item);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerKeyNo.setAdapter(adapter);

        floatingActionButton = (FloatingActionButton) v.findViewById(R.id.fab_keyex);
        floatingActionButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Get a strong reference to the activity.
                MainActivity activity = KeyExchangeFragment.this.activity.get();
                if (activity == null) {
                    // The activity has already been garbage collected.
                    return;
                }

                Message msg;
                msg = activity.applicationEventHandler.obtainMessage(
                        MainActivity.ApplicationEventHandler.START_KEYEXCHANGE);
                int selectedKeyNo = spinnerKeyNo.getSelectedItemPosition();
                if (selectedKeyNo == Spinner.INVALID_POSITION) {
                    Toast.makeText(KeyExchangeFragment.this.getContext(),
                            R.string.err_invalid_keyno, Toast.LENGTH_LONG).show();
                } else {
                    msg.arg1 = selectedKeyNo;
                    activity.applicationEventHandler.sendMessage(msg);
                }
            }
        });

        textViewChecksum = (TextView) v.findViewById(R.id.textview_checksum);
        textViewChecksum.setText("");

        buttonConfirmKey = (Button) v.findViewById(R.id.button_keyex_confirm);
        buttonConfirmKey.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                // Get a strong reference to the activity.
                MainActivity activity = KeyExchangeFragment.this.activity.get();
                if (activity == null) {
                    // The activity has already been garbage collected.
                    return;
                }

                Message msg;
                msg = activity.applicationEventHandler.obtainMessage(
                        MainActivity.ApplicationEventHandler.USER_KEY_CONFIRMED);
                activity.applicationEventHandler.sendMessage(msg);
            }
        });

        textViewInstructions= (TextView) v.findViewById(R.id.textview_keyex_instructions);
        textViewInstructions.setText(Html.fromHtml(getString(R.string.keyex_instructions)));

        msgHandler = new MessageHandler(this);

        return v;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        this.activity = new WeakReference<MainActivity>((MainActivity) activity);
    }
}
