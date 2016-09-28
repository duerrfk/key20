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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;

import java.lang.ref.WeakReference;

/**
 * Fragment displaying the UI for unlocking a door.
 */
public class UnlockFragment extends Fragment {

    private FloatingActionButton floatingActionButton;
    private ProgressBar progressBar;

    private WeakReference<MainActivity> activity;

    public MessageHandler msgHandler;

    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     *
     * @return A new instance of fragment UnlockFragment.
     */
    public static UnlockFragment newInstance() {
        UnlockFragment fragment = new UnlockFragment();

        Bundle args = new Bundle();
        fragment.setArguments(args);

        return fragment;
    }

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
        public final static int TASK_STARTED = 0;
        public final static int TASK_FINISHED = 1;

        private final WeakReference<UnlockFragment> fragment;

        public MessageHandler(UnlockFragment fragment) {
            this.fragment = new WeakReference<UnlockFragment>(fragment);
        }

        @Override
        public void handleMessage(Message message) {
            // Get a strong reference to the fragment.
            UnlockFragment f = fragment.get();
            if (f == null) {
                // The fragment has been garbage-collected.
                return;
            }

            switch (message.what) {
                case TASK_STARTED :
                    f.progressBar.setVisibility(View.VISIBLE);
                    break;
                case TASK_FINISHED :
                    f.progressBar.setVisibility(View.INVISIBLE);
                    break;
            }
        }
    }

    /**
     * An empty constructor is required.
     */
    public UnlockFragment() {
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        View v = inflater.inflate(R.layout.fragment_unlock, container, false);

        progressBar = (ProgressBar) v.findViewById(R.id.progressbar_unlock);
        progressBar.setVisibility(View.INVISIBLE);

        floatingActionButton = (FloatingActionButton) v.findViewById(R.id.fab_unlock);
        floatingActionButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                // Get a strong reference to the activity.
                MainActivity activity = UnlockFragment.this.activity.get();
                if (activity == null) {
                    // The activity has already been garbage collected.
                    return;
                }

                Message msg;
                msg = activity.applicationEventHandler.obtainMessage(
                        MainActivity.ApplicationEventHandler.START_DOOR_UNLOCK);
                activity.applicationEventHandler.sendMessage(msg);
            }
        });

        msgHandler = new MessageHandler(this);

        return v;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        this.activity = new WeakReference<MainActivity>((MainActivity) activity);
    }
}
