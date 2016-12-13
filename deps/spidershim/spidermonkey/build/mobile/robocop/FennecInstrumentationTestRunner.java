/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Bundle;
import android.test.InstrumentationTestRunner;
import android.util.Log;

public class FennecInstrumentationTestRunner extends InstrumentationTestRunner {
    private static Bundle sArguments;

    @Override
    public void onCreate(Bundle arguments) {
        sArguments = arguments;
        if (sArguments == null) {
            Log.e("Robocop", "FennecInstrumentationTestRunner.onCreate got null bundle");
        }
        super.onCreate(arguments);
    }

    // unfortunately we have to make this static because test classes that don't extend
    // from ActivityInstrumentationTestCase2 can't get a reference to this class.
    public static Bundle getFennecArguments() {
        if (sArguments == null) {
            Log.e("Robocop", "FennecInstrumentationTestCase.getFennecArguments returns null bundle");
        }
        return sArguments;
    }
}
