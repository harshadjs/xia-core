#!/usr/local/sbin/click-install -uct12
require(library xia_mq_template.click);

gen_fb1_0(HID2, CID0, HID4, eth2, eth4);
gen_fb1_0(HID3, CID0, HID5, eth3, eth5);
gen_fb1_0(HID4, CID0, HID2, eth4, eth2);
gen_fb1_0(HID5, CID0, HID3, eth5, eth3);
