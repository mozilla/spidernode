/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.net.wifi;

import android.os.Parcelable;
import android.os.Parcel;

import java.util.BitSet;

/**
 * A class representing a configured Wi-Fi network, including the
 * security configuration. Android will not necessarily support
 * all of these security schemes initially.
 */
public class WifiConfiguration implements Parcelable {

    /** {@hide} */
    public static final String ssidVarName = "ssid";
    /** {@hide} */
    public static final String bssidVarName = "bssid";
    /** {@hide} */
    public static final String pskVarName = "psk";
    /** {@hide} */
    public static final String[] wepKeyVarNames = { "wep_key0", "wep_key1", "wep_key2", "wep_key3" };
    /** {@hide} */
    public static final String wepTxKeyIdxVarName = "wep_tx_keyidx";
    /** {@hide} */
    public static final String priorityVarName = "priority";
    /** {@hide} */
    public static final String hiddenSSIDVarName = "scan_ssid";

    public class EnterpriseField {
        private String varName;
        private String value;

        private EnterpriseField(String varName) {
            this.varName = varName;
            this.value = null;
        }

        public void setValue(String value) {
            this.value = value;
        }

        public String varName() {
            return varName;
        }

        public String value() {
            return value;
        }
    }

    public EnterpriseField eap = new EnterpriseField("eap");
    public EnterpriseField phase2 = new EnterpriseField("phase2");
    public EnterpriseField identity = new EnterpriseField("identity");
    public EnterpriseField anonymous_identity = new EnterpriseField("anonymous_identity");
    public EnterpriseField password = new EnterpriseField("password");
    public EnterpriseField client_cert = new EnterpriseField("client_cert");
    public EnterpriseField private_key = new EnterpriseField("private_key");
    public EnterpriseField ca_cert = new EnterpriseField("ca_cert");

    public EnterpriseField[] enterpriseFields = {
            eap, phase2, identity, anonymous_identity, password, client_cert,
            private_key, ca_cert };

    /**
     * Recognized key management schemes.
     */
    public static class KeyMgmt {
        private KeyMgmt() { }

        /** WPA is not used; plaintext or static WEP could be used. */
        public static final int NONE = 0;
        /** WPA pre-shared key (requires {@code preSharedKey} to be specified). */
        public static final int WPA_PSK = 1;
        /** WPA using EAP authentication. Generally used with an external authentication server. */
        public static final int WPA_EAP = 2;
        /** IEEE 802.1X using EAP authentication and (optionally) dynamically
         * generated WEP keys. */
        public static final int IEEE8021X = 3;

        public static final String varName = "key_mgmt";

        public static final String[] strings = { "NONE", "WPA_PSK", "WPA_EAP", "IEEE8021X" };
    }

    /**
     * Recognized security protocols.
     */
    public static class Protocol {
        private Protocol() { }

        /** WPA/IEEE 802.11i/D3.0 */
        public static final int WPA = 0;
        /** WPA2/IEEE 802.11i */
        public static final int RSN = 1;

        public static final String varName = "proto";

        public static final String[] strings = { "WPA", "RSN" };
    }

    /**
     * Recognized IEEE 802.11 authentication algorithms.
     */
    public static class AuthAlgorithm {
        private AuthAlgorithm() { }

        /** Open System authentication (required for WPA/WPA2) */
        public static final int OPEN = 0;
        /** Shared Key authentication (requires static WEP keys) */
        public static final int SHARED = 1;
        /** LEAP/Network EAP (only used with LEAP) */
        public static final int LEAP = 2;

        public static final String varName = "auth_alg";

        public static final String[] strings = { "OPEN", "SHARED", "LEAP" };
    }

    /**
     * Recognized pairwise ciphers for WPA.
     */
    public static class PairwiseCipher {
        private PairwiseCipher() { }

        /** Use only Group keys (deprecated) */
        public static final int NONE = 0;
        /** Temporal Key Integrity Protocol [IEEE 802.11i/D7.0] */
        public static final int TKIP = 1;
        /** AES in Counter mode with CBC-MAC [RFC 3610, IEEE 802.11i/D7.0] */
        public static final int CCMP = 2;

        public static final String varName = "pairwise";

        public static final String[] strings = { "NONE", "TKIP", "CCMP" };
    }

    /**
     * Recognized group ciphers.
     * <pre>
     * CCMP = AES in Counter mode with CBC-MAC [RFC 3610, IEEE 802.11i/D7.0]
     * TKIP = Temporal Key Integrity Protocol [IEEE 802.11i/D7.0]
     * WEP104 = WEP (Wired Equivalent Privacy) with 104-bit key
     * WEP40 = WEP (Wired Equivalent Privacy) with 40-bit key (original 802.11)
     * </pre>
     */
    public static class GroupCipher {
        private GroupCipher() { }

        /** WEP40 = WEP (Wired Equivalent Privacy) with 40-bit key (original 802.11) */
        public static final int WEP40 = 0;
        /** WEP104 = WEP (Wired Equivalent Privacy) with 104-bit key */
        public static final int WEP104 = 1;
        /** Temporal Key Integrity Protocol [IEEE 802.11i/D7.0] */
        public static final int TKIP = 2;
        /** AES in Counter mode with CBC-MAC [RFC 3610, IEEE 802.11i/D7.0] */
        public static final int CCMP = 3;

        public static final String varName = "group";

        public static final String[] strings = { "WEP40", "WEP104", "TKIP", "CCMP" };
    }

    /** Possible status of a network configuration. */
    public static class Status {
        private Status() { }

        /** this is the network we are currently connected to */
        public static final int CURRENT = 0;
        /** supplicant will not attempt to use this network */
        public static final int DISABLED = 1;
        /** supplicant will consider this network available for association */
        public static final int ENABLED = 2;

        public static final String[] strings = { "current", "disabled", "enabled" };
    }

    /**
     * The ID number that the supplicant uses to identify this
     * network configuration entry. This must be passed as an argument
     * to most calls into the supplicant.
     */
    public int networkId;

    /**
     * The current status of this network configuration entry.
     * @see Status
     */
    public int status;
    /**
     * The network's SSID. Can either be an ASCII string,
     * which must be enclosed in double quotation marks
     * (e.g., {@code &quot;MyNetwork&quot;}, or a string of
     * hex digits,which are not enclosed in quotes
     * (e.g., {@code 01a243f405}).
     */
    public String SSID;
    /**
     * When set, this network configuration entry should only be used when
     * associating with the AP having the specified BSSID. The value is
     * a string in the format of an Ethernet MAC address, e.g.,
     * <code>XX:XX:XX:XX:XX:XX</code> where each <code>X</code> is a hex digit.
     */
    public String BSSID;

    /**
     * Pre-shared key for use with WPA-PSK.
     * <p/>
     * When the value of this key is read, the actual key is
     * not returned, just a "*" if the key has a value, or the null
     * string otherwise.
     */
    public String preSharedKey;
    /**
     * Up to four WEP keys. Either an ASCII string enclosed in double
     * quotation marks (e.g., {@code &quot;abcdef&quot;} or a string
     * of hex digits (e.g., {@code 0102030405}).
     * <p/>
     * When the value of one of these keys is read, the actual key is
     * not returned, just a "*" if the key has a value, or the null
     * string otherwise.
     */
    public String[] wepKeys;

    /** Default WEP key index, ranging from 0 to 3. */
    public int wepTxKeyIndex;

    /**
     * Priority determines the preference given to a network by {@code wpa_supplicant}
     * when choosing an access point with which to associate.
     */
    public int priority;

    /**
     * This is a network that does not broadcast its SSID, so an
     * SSID-specific probe request must be used for scans.
     */
    public boolean hiddenSSID;

    /**
     * The set of key management protocols supported by this configuration.
     * See {@link KeyMgmt} for descriptions of the values.
     * Defaults to WPA-PSK WPA-EAP.
     */
    public BitSet allowedKeyManagement;
    /**
     * The set of security protocols supported by this configuration.
     * See {@link Protocol} for descriptions of the values.
     * Defaults to WPA RSN.
     */
    public BitSet allowedProtocols;
    /**
     * The set of authentication protocols supported by this configuration.
     * See {@link AuthAlgorithm} for descriptions of the values.
     * Defaults to automatic selection.
     */
    public BitSet allowedAuthAlgorithms;
    /**
     * The set of pairwise ciphers for WPA supported by this configuration.
     * See {@link PairwiseCipher} for descriptions of the values.
     * Defaults to CCMP TKIP.
     */
    public BitSet allowedPairwiseCiphers;
    /**
     * The set of group ciphers supported by this configuration.
     * See {@link GroupCipher} for descriptions of the values.
     * Defaults to CCMP TKIP WEP104 WEP40.
     */
    public BitSet allowedGroupCiphers;


    public WifiConfiguration() {
        networkId = -1;
        SSID = null;
        BSSID = null;
        priority = 0;
        hiddenSSID = false;
        allowedKeyManagement = new BitSet();
        allowedProtocols = new BitSet();
        allowedAuthAlgorithms = new BitSet();
        allowedPairwiseCiphers = new BitSet();
        allowedGroupCiphers = new BitSet();
        wepKeys = new String[4];
        for (int i = 0; i < wepKeys.length; i++)
            wepKeys[i] = null;
        for (EnterpriseField field : enterpriseFields) {
            field.setValue(null);
        }
    }

    public String toString() {
        StringBuilder sbuf = new StringBuilder();
        if (this.status == WifiConfiguration.Status.CURRENT) {
            sbuf.append("* ");
        } else if (this.status == WifiConfiguration.Status.DISABLED) {
            sbuf.append("- ");
        }
        sbuf.append("ID: ").append(this.networkId).append(" SSID: ").append(this.SSID).
                append(" BSSID: ").append(this.BSSID).append(" PRIO: ").append(this.priority).
                append('\n');
        sbuf.append(" KeyMgmt:");
        for (int k = 0; k < this.allowedKeyManagement.size(); k++) {
            if (this.allowedKeyManagement.get(k)) {
                sbuf.append(" ");
                if (k < KeyMgmt.strings.length) {
                    sbuf.append(KeyMgmt.strings[k]);
                } else {
                    sbuf.append("??");
                }
            }
        }
        sbuf.append(" Protocols:");
        for (int p = 0; p < this.allowedProtocols.size(); p++) {
            if (this.allowedProtocols.get(p)) {
                sbuf.append(" ");
                if (p < Protocol.strings.length) {
                    sbuf.append(Protocol.strings[p]);
                } else {
                    sbuf.append("??");
                }
            }
        }
        sbuf.append('\n');
        sbuf.append(" AuthAlgorithms:");
        for (int a = 0; a < this.allowedAuthAlgorithms.size(); a++) {
            if (this.allowedAuthAlgorithms.get(a)) {
                sbuf.append(" ");
                if (a < AuthAlgorithm.strings.length) {
                    sbuf.append(AuthAlgorithm.strings[a]);
                } else {
                    sbuf.append("??");
                }
            }
        }
        sbuf.append('\n');
        sbuf.append(" PairwiseCiphers:");
        for (int pc = 0; pc < this.allowedPairwiseCiphers.size(); pc++) {
            if (this.allowedPairwiseCiphers.get(pc)) {
                sbuf.append(" ");
                if (pc < PairwiseCipher.strings.length) {
                    sbuf.append(PairwiseCipher.strings[pc]);
                } else {
                    sbuf.append("??");
                }
            }
        }
        sbuf.append('\n');
        sbuf.append(" GroupCiphers:");
        for (int gc = 0; gc < this.allowedGroupCiphers.size(); gc++) {
            if (this.allowedGroupCiphers.get(gc)) {
                sbuf.append(" ");
                if (gc < GroupCipher.strings.length) {
                    sbuf.append(GroupCipher.strings[gc]);
                } else {
                    sbuf.append("??");
                }
            }
        }
        sbuf.append('\n').append(" PSK: ");
        if (this.preSharedKey != null) {
            sbuf.append('*');
        }

        for (EnterpriseField field : enterpriseFields) {
            sbuf.append('\n').append(" " + field.varName() + ": ");
            String value = field.value();
            if (value != null) sbuf.append(value);
        }
        sbuf.append('\n');
        return sbuf.toString();
    }

    /**
     * Construct a WifiConfiguration from a scanned network
     * @param scannedAP the scan result used to construct the config entry
     * TODO: figure out whether this is a useful way to construct a new entry.
     *
    public WifiConfiguration(ScanResult scannedAP) {
        networkId = -1;
        SSID = scannedAP.SSID;
        BSSID = scannedAP.BSSID;
    }
    */

    private static BitSet readBitSet(Parcel src) {
        int cardinality = src.readInt();

        BitSet set = new BitSet();
        for (int i = 0; i < cardinality; i++)
            set.set(src.readInt());

        return set;
    }

    private static void writeBitSet(Parcel dest, BitSet set) {
        int nextSetBit = -1;

        dest.writeInt(set.cardinality());

        while ((nextSetBit = set.nextSetBit(nextSetBit + 1)) != -1)
            dest.writeInt(nextSetBit);
    }

    /** Implement the Parcelable interface {@hide} */
    public int describeContents() {
        return 0;
    }

    /** Implement the Parcelable interface {@hide} */
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(networkId);
        dest.writeInt(status);
        dest.writeString(SSID);
        dest.writeString(BSSID);
        dest.writeString(preSharedKey);
        for (String wepKey : wepKeys)
            dest.writeString(wepKey);
        dest.writeInt(wepTxKeyIndex);
        dest.writeInt(priority);
        dest.writeInt(hiddenSSID ? 1 : 0);

        writeBitSet(dest, allowedKeyManagement);
        writeBitSet(dest, allowedProtocols);
        writeBitSet(dest, allowedAuthAlgorithms);
        writeBitSet(dest, allowedPairwiseCiphers);
        writeBitSet(dest, allowedGroupCiphers);

        for (EnterpriseField field : enterpriseFields) {
            dest.writeString(field.value());
        }
    }

    /** Implement the Parcelable interface {@hide} */
    public static final Creator<WifiConfiguration> CREATOR =
        new Creator<WifiConfiguration>() {
            public WifiConfiguration createFromParcel(Parcel in) {
                WifiConfiguration config = new WifiConfiguration();
                config.networkId = in.readInt();
                config.status = in.readInt();
                config.SSID = in.readString();
                config.BSSID = in.readString();
                config.preSharedKey = in.readString();
                for (int i = 0; i < config.wepKeys.length; i++)
                    config.wepKeys[i] = in.readString();
                config.wepTxKeyIndex = in.readInt();
                config.priority = in.readInt();
                config.hiddenSSID = in.readInt() != 0;
                config.allowedKeyManagement   = readBitSet(in);
                config.allowedProtocols       = readBitSet(in);
                config.allowedAuthAlgorithms  = readBitSet(in);
                config.allowedPairwiseCiphers = readBitSet(in);
                config.allowedGroupCiphers    = readBitSet(in);

                for (EnterpriseField field : config.enterpriseFields) {
                    field.setValue(in.readString());
                }
                return config;
            }

            public WifiConfiguration[] newArray(int size) {
                return new WifiConfiguration[size];
            }
        };
}
