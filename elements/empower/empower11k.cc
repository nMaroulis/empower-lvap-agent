/*
 * empower11k.{cc,hh} -- Send and parse 802.11k messages.
 * Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/args.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include "empowerpacket.hh"
#include "empowerlvapmanager.hh"
#include "empower11k.hh"
CLICK_DECLS

Empower11k::Empower11k() :
		_el(0), _debug(false) {
}

Empower11k::~Empower11k() {
}

int Empower11k::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
              .read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			  .read("DEBUG", _debug).complete();

	return ret;

}

void Empower11k::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_wifi)) {
		click_chatter("%{element} :: %s :: Packet too small: %d Vs. %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct click_wifi));
		p->kill();
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	uint8_t iface_id = PAINT_ANNO(p);
	uint8_t type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;

	if (type != WIFI_FC0_TYPE_MGT) {
		click_chatter("%{element} :: %s :: Received non-management packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

	if (subtype != WIFI_FC0_SUBTYPE_ACTION) {
		click_chatter("%{element} :: %s :: Received non-action packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	EtherAddress src = EtherAddress(w->i_addr2);

	EmpowerStationState *ess = _el->lvaps()->get_pointer(src);

	//If we're not aware of this LVAP, ignore
	if (!ess) {
		click_chatter("%{element} :: %s :: Unknown station %s",
				      this,
				      __func__,
				      src.unparse().c_str());
		p->kill();
		return;
	}

	// if this is an uplink only lvap then ignore request
	if (!ess->_set_mask) {
		p->kill();
		return;
	}

    // If auth request is coming from different channel, ignore
	if (ess->_iface_id != iface_id) {
		click_chatter("%{element} :: %s :: %s is on iface %u, message coming from %u",
				      this,
				      __func__,
				      src.unparse().c_str(),
					  ess->_iface_id,
					  iface_id);
		p->kill();
		return;
	}

	EtherAddress bssid = EtherAddress(w->i_addr3);

	click_chatter("%{element} :: %s :: %s management action from %s bssd %s",
			      this,
			      __func__,
			      src.unparse().c_str(),
				  bssid.unparse().c_str());

	p->kill();
	return;

}

void Empower11k::send_neighbor_report_request(EtherAddress sta, uint8_t token) {

    EmpowerStationState *ess = _el->lvaps()->get_pointer(sta);

	if (_debug) {
		click_chatter("%{element} :: %s :: sending neighbor report request to %s token %u",
				      this,
				      __func__,
				      sta.unparse().c_str(),
				      token);
	}

	int len = sizeof(struct click_wifi) +
		1 + /* category */
		1 + /* action */
		1 + /* dialog token */
		0;

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_ACTION;
	w->i_fc[1] = WIFI_FC1_DIR_NODS;

	memcpy(w->i_addr1, sta.data(), 6);
	memcpy(w->i_addr2, ess->_lvap_bssid.data(), 6);
	memcpy(w->i_addr3, ess->_lvap_bssid.data(), 6);

	w->i_dur = 0;
	w->i_seq = 0;

	uint8_t *ptr;

	ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	*(uint8_t *) ptr = 5; /* radio measurement */
	ptr += 1;

	*(uint8_t *) ptr = 4; /* neighbor request */
	ptr += 1;

	*(uint8_t *) ptr = token; /* dialog token */

	output(0).push(p);

}

enum {
	H_DEBUG,
	H_NEIGHBOR_REPORT_REQUEST,
};

String Empower11k::read_handler(Element *e, void *thunk) {
	Empower11k *td = (Empower11k *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int Empower11k::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	Empower11k *f = (Empower11k *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {    //debug
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	case H_NEIGHBOR_REPORT_REQUEST: {

		Vector<String> tokens;
		cp_spacevec(s, tokens);

		if (tokens.size() != 2)
			return errh->error("ports send_neighbor_report_request 2 parameters");

		EtherAddress sta;
		uint8_t token;

		if (!EtherAddressArg().parse(tokens[0], sta)) {
			return errh->error("error param %s: must start with an Ethernet address", tokens[0].c_str());
		}

		if (!IntArg().parse(tokens[1], token)) {
			return errh->error("error param %s: must start with an int", tokens[1].c_str());
		}

		f->send_neighbor_report_request(sta, token);

		break;

	}
	}
	return 0;
}

void Empower11k::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
	add_write_handler("send_neighbor_report_request", write_handler, (void *) H_NEIGHBOR_REPORT_REQUEST);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Empower11k)