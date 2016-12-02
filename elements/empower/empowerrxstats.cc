/*
 * empowerrxstats.{cc,hh} -- tracks rx stats
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <elements/wifi/bitrate.hh>
#include "empowerlvapmanager.hh"
#include "empowerrxstats.hh"
CLICK_DECLS

void send_summary_trigger_callback(Timer *timer, void *data) {
	// send summary
	SummaryTrigger *summary = (SummaryTrigger *) data;
	summary->_el->send_summary_trigger(summary);
	summary->_sent++;
	if (summary->_limit > 0 && summary->_sent >= (unsigned) summary->_limit) {
		summary->_ers->del_summary_trigger(summary->_trigger_id);
		return;
	}
	// re-schedule the timer
	timer->schedule_after_msec(summary->_period);
}

void send_rssi_trigger_callback(Timer *timer, void *data) {
	// process triggers
	RssiTrigger *rssi = (RssiTrigger *) data;
	rssi->_ers->lock.acquire_read();
	for (NTIter iter = rssi->_ers->stas.begin(); iter.live(); iter++) {
		DstInfo *nfo = &iter.value();
		// not matching the address
		if (nfo->_eth != rssi->_eth) {
			continue;
		}
		// check if condition matches
		if (rssi->matches(nfo) && !rssi->_dispatched) {
			rssi->_el->send_rssi_trigger(rssi->_trigger_id, nfo->_iface_id, nfo->_sma_rssi->avg());
			rssi->_dispatched = true;
		} else if (!rssi->matches(nfo) && rssi->_dispatched) {
			rssi->_dispatched = false;
		}
	}
	rssi->_ers->lock.release_read();
	// re-schedule the timer
	timer->schedule_after_msec(rssi->_period);
}


//------------------------------------- DRP STUFF -----------------------------------
void send_drp_trigger_callback(Timer *timer, void *data) {
    /* debug */
    click_chatter("DRP :: empowerrxstats.cc :: %s", __func__);
    /* - - - */
	DrpTrigger *drp = (DrpTrigger *) data;
	drp->_ers->lock.acquire_read();
	for (NTIter iter = drp->_ers->stas.begin(); iter.live(); iter++) {
		DstInfo *nfo = &iter.value();
        //if (nfo->_eth != drp->_eth) { continue; }
        //else{
        //    drp->_dispatched = false;
        //}
        if(!drp->_dispatched) {
            click_chatter("DRP :: empowerrxstats.cc :: %s , dispached %d  {nfo_iface %d}", __func__, drp->_dispatched,
                          nfo->_iface_id);
            drp->_el->send_drp_trigger(drp->_trigger_id, nfo->_iface_id);
            drp->_dispatched = true;
        }
	}
	drp->_ers->lock.release_read();
	timer->schedule_after_msec(drp->_period);// re-schedule the timer
}

void EmpowerRXStats::add_drp_trigger(EtherAddress eth, uint32_t trigger_id, uint16_t period) {
    /* debug */
    click_chatter("DRP :: empowerrxstats.cc :: %s (str: )", __func__);
    /* - - - */
    DrpTrigger * drp = new DrpTrigger(eth, trigger_id, period , false , _el, this);
    for (DRPIter qi = _drp_triggers.begin(); qi != _drp_triggers.end(); qi++) {
        if (*drp== **qi) {
            click_chatter("%{element} :: %s :: trigger already defined (%s), setting sent to false",
                          this,
                          __func__,
                          drp->unparse().c_str());
            (*qi)->_dispatched = false;
            return;
        }
    }
	drp->_trigger_timer->assign(&send_drp_trigger_callback, (void *) drp);
	drp->_trigger_timer->initialize(this);
	drp->_trigger_timer->schedule_now();
	_drp_triggers.push_back(drp);
}

void EmpowerRXStats::del_drp_trigger(uint32_t drp_id) {
    for (DRPIter qi = _drp_triggers.begin(); qi != _drp_triggers.end(); qi++) {
        if ((*qi)->_trigger_id == drp_id) {
            (*qi)->_trigger_timer->clear();
            _drp_triggers.erase(qi);
            delete *qi;
            break;
        }
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EmpowerRXStats::EmpowerRXStats() :
		_el(0), _timer(this), _signal_offset(0), _period(1000),
		_sma_period(13), _max_silent_window_count(10), _rssi_threshold(-70),
		_debug(false) {

}

EmpowerRXStats::~EmpowerRXStats() {
}

int EmpowerRXStats::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

int EmpowerRXStats::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret = Args(conf, this, errh)
			.read("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("SMA_PERIOD", _sma_period)
			.read("SIGNAL_OFFSET", _signal_offset)
			.read("PERIOD", _period)
			.read("DEBUG", _debug)
			.complete();

	return ret;

}

void EmpowerRXStats::run_timer(Timer *) {
	// process stations
	lock.acquire_write();
	for (NTIter iter = stas.begin(); iter.live();) {
		// Update stats
		DstInfo *nfo = &iter.value();
		nfo->update();
		// Delete stale entries
		if (nfo->_silent_window_count > _max_silent_window_count) {
			iter = stas.erase(iter);
		} else {
			++iter;
		}
	}
	// process access points
	for (NTIter iter = aps.begin(); iter.live();) {
		// Update aps
		DstInfo *nfo = &iter.value();
		nfo->update();
		// Delete stale entries
		if (nfo->_silent_window_count > _max_silent_window_count) {
			iter = aps.erase(iter);
		} else {
			++iter;
		}
	}
	// process links
	for (CIter iter = links.begin(); iter.live();) {
		// Update estimator
		CqmLink *nfo = &iter.value();
		nfo->estimator();
		// Delete stale entries
		if (nfo->silentWindowCount> _max_silent_window_count) {
			iter = links.erase(iter);
		} else {
			++iter;
		}
	}
	lock.release_write();
	// rescheduler
	_timer.schedule_after_msec(_period);
}

Packet *
EmpowerRXStats::simple_action(Packet *p) {

	struct click_wifi *w = (struct click_wifi *) p->data();

	unsigned wifi_header_size = sizeof(struct click_wifi);

	if ((w->i_fc[1] & WIFI_FC1_DIR_MASK) == WIFI_FC1_DIR_DSTODS)
		wifi_header_size += WIFI_ADDR_LEN;

	if (WIFI_QOS_HAS_SEQ(w))
		wifi_header_size += sizeof(uint16_t);

	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	if ((ceh->magic == WIFI_EXTRA_MAGIC) && ceh->pad && (wifi_header_size & 3))
		wifi_header_size += 4 - (wifi_header_size & 3);

	if (p->length() < wifi_header_size) {
		return p;
	}

	int dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
	int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
	int subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;
	int retry = w->i_fc[1] & WIFI_FC1_RETRY;
	bool station = false;

	// Discard frames that do not have sequence numbers OR are retry frames
	// These conditions are to enable using the sequence number to estimate packet delivery rate
	if (retry == 1 || type == WIFI_FC0_TYPE_CTL) {
		return p;
	}

	switch (dir) {
	case WIFI_FC1_DIR_TODS:
		// TODS bit not set when TA is an access point, but only when TA is a station
		station = true;
		break;
	case WIFI_FC1_DIR_NODS:
		if (type == WIFI_FC0_TYPE_DATA) {
			// NODS never set for data frames unless in ad-hoc mode
			station = true;
			break;
		} else if (type == WIFI_FC0_TYPE_MGT) {
			if (subtype == WIFI_FC0_SUBTYPE_BEACON
					|| subtype == WIFI_FC0_SUBTYPE_PROBE_RESP) {
				// NODS set for beacon frames and probe response from access points
				station = false;
				break;
			} else if (subtype == WIFI_FC0_SUBTYPE_PROBE_REQ
					|| subtype == WIFI_FC0_SUBTYPE_REASSOC_REQ
					|| subtype == WIFI_FC0_SUBTYPE_ASSOC_REQ
					|| subtype == WIFI_FC0_SUBTYPE_AUTH
					|| subtype == WIFI_FC0_SUBTYPE_DISASSOC
					|| subtype == WIFI_FC0_SUBTYPE_DEAUTH) {
				// NODS set for beacon frames and probe response from access points
				station = true;
				break;
			}
		}
		// no idea, ignore packet
		return p;
	case WIFI_FC1_DIR_FROMDS:
		// FROMDS bit not set when TA is an station, but only when TA is an access point
		station = false;
		break;
	case WIFI_FC1_DIR_DSTODS:
		// DSTODS bit never set
		station = false;
		break;
	}

	EtherAddress ra = EtherAddress(w->i_addr1);
	EtherAddress ta = EtherAddress(w->i_addr2);

	int8_t rssi;
	memcpy(&rssi, &ceh->rssi, 1);

	uint8_t iface_id = PAINT_ANNO(p);

	// create frame meta-data
	Frame *frame = new Frame(ra, ta, ceh->tsft, ceh->flags, w->i_seq, rssi, ceh->rate, type, subtype, p->length(), retry, station, iface_id);

	if (_debug) {
		click_chatter("%{element} :: %s :: %s", this, __func__, frame->unparse().c_str());
	}

	lock.acquire_write();

	update_neighbor(frame);
	update_link_table(frame);

	// check if frame meta-data should be saved
	for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
		if ((*qi)->_iface != iface_id) {
			continue;
		}
		if ((*qi)->_eth == ta || (*qi)->_eth.is_broadcast()) {
			(*qi)->_frames.push_back(frame);
		}
	}

	lock.release_write();

	return p;

}

void EmpowerRXStats::update_link_table(Frame *frame) {

	// Update channel quality map
	CqmLink *nfo;
	nfo = links.get_pointer(frame->_ta);
	if (!nfo) {
		links[frame->_ta] = CqmLink();
		nfo = links.get_pointer(frame->_ta);
		nfo->senderType = frame->_station ? 0 : 1;
		nfo->sourceAddr = frame->_ta;
		nfo->lastEstimateTime = Timestamp::now();
		nfo->currentTime = Timestamp::now();
		nfo->lastSeqNum = frame->_seq - 1;
		nfo->currentSeqNum = frame->_seq;
		nfo->xi = 0;
		nfo->ifaceId = frame->_iface_id;
		nfo->rssiThreshold = _rssi_threshold;
	}

	// Add sample
	nfo->add_sample(frame);

}

void EmpowerRXStats::update_neighbor(Frame *frame) {

	DstInfo *nfo;

	if (frame->_station) {
		nfo = stas.get_pointer(frame->_ta);
	} else {
		nfo = aps.get_pointer(frame->_ta);
	}

	if (!nfo) {
		if (frame->_station) {
			stas[frame->_ta] = DstInfo();
			nfo = stas.get_pointer(frame->_ta);
			nfo->_sma_rssi = new SMA(_sma_period);
			nfo->_iface_id = frame->_iface_id;
			nfo->_eth = frame->_ta;
		} else {
			aps[frame->_ta] = DstInfo();
			nfo = aps.get_pointer(frame->_ta);
			nfo->_sma_rssi = new SMA(_sma_period);
			nfo->_iface_id = frame->_iface_id;
			nfo->_eth = frame->_ta;
		}
	}

	// Add sample
	nfo->add_sample(frame);

}

void EmpowerRXStats::add_rssi_trigger(EtherAddress eth, uint32_t trigger_id, empower_rssi_trigger_relation rel, int val, uint16_t period) {
	RssiTrigger * rssi = new RssiTrigger(eth, trigger_id, rel, val, false, period, _el, this);
	for (RTIter qi = _rssi_triggers.begin(); qi != _rssi_triggers.end(); qi++) {
		if (*rssi== **qi) {
			click_chatter("%{element} :: %s :: trigger already defined (%s), setting sent to false",
						  this,
						  __func__,
						  rssi->unparse().c_str());
			(*qi)->_dispatched = false;
			return;
		}
	}
	rssi->_trigger_timer->assign(&send_rssi_trigger_callback, (void *) rssi);
	rssi->_trigger_timer->initialize(this);
	rssi->_trigger_timer->schedule_now();
	_rssi_triggers.push_back(rssi);
}

void EmpowerRXStats::del_rssi_trigger(uint32_t trigger_id) {
	for (RTIter qi = _rssi_triggers.begin(); qi != _rssi_triggers.end(); qi++) {
		if ((*qi)->_trigger_id == trigger_id) {
			(*qi)->_trigger_timer->clear();
			_rssi_triggers.erase(qi);
			break;
		}
	}
}

void EmpowerRXStats::clear_triggers() {
	// clear rssi triggers
	for (RTIter qi = _rssi_triggers.begin(); qi != _rssi_triggers.end(); qi++) {
		(*qi)->_trigger_timer->clear();
	}
	_rssi_triggers.clear();
	// clear summary triggers
	for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
		(*qi)->_trigger_timer->clear();
		delete *qi;
	}
	_summary_triggers.clear();
	//-----------  clear drp triggers -----------------
	for (DRPIter qi = _drp_triggers.begin(); qi != _drp_triggers.end(); qi++) {
		(*qi)->_trigger_timer->clear();
	}
	_drp_triggers.clear();
}

void EmpowerRXStats::add_summary_trigger(int iface, EtherAddress addr, uint32_t summary_id, int16_t limit, uint16_t period) {
	SummaryTrigger * summary = new SummaryTrigger(iface, addr, summary_id, limit, period, _el, this);
	for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
		if (*summary == **qi) {
			click_chatter("%{element} :: %s :: summary already defined (%s), ignoring",
						  this,
						  __func__,
						  summary->unparse().c_str());
			return;
		}
	}
	summary->_trigger_timer->assign(&send_summary_trigger_callback, (void *) summary);
	summary->_trigger_timer->initialize(this);
	summary->_trigger_timer->schedule_now();
	_summary_triggers.push_back(summary);
}

void EmpowerRXStats::del_summary_trigger(uint32_t summary_id) {
	for (DTIter qi = _summary_triggers.begin(); qi != _summary_triggers.end(); qi++) {
		if ((*qi)->_trigger_id == summary_id) {
			(*qi)->_trigger_timer->clear();
			_summary_triggers.erase(qi);
			delete *qi;
			break;
		}
	}
}

enum {
	H_DEBUG,
	H_NEIGHBORS,
	H_LINKS,
	H_RESET,
	H_SIGNAL_OFFSET,
	H_MATCHES,
	H_RSSI_TRIGGERS,
	H_SUMMARY_TRIGGERS,
    H_DRP_TRIGGERS
};

String EmpowerRXStats::read_handler(Element *e, void *thunk) {

	EmpowerRXStats *td = (EmpowerRXStats *) e;

	switch ((uintptr_t) thunk) {
	case H_MATCHES: {
		StringAccum sa;
		for (RTIter qi = td->_rssi_triggers.begin(); qi != td->_rssi_triggers.end(); qi++) {
			for (NTIter iter = td->stas.begin(); iter.live(); iter++) {
				DstInfo *nfo = &iter.value();
				if (!nfo)
					continue;
				if ((*qi)->matches(nfo)) {
					sa << (*qi)->unparse();
					sa << " current " << nfo->_sma_rssi->avg();
					sa << "\n";
				}
			}
		}
		return sa.take_string();
	}
	case H_RSSI_TRIGGERS: {
		StringAccum sa;
		for (RTIter qi = td->_rssi_triggers.begin(); qi != td->_rssi_triggers.end(); qi++) {
			sa << (*qi)->unparse() << "\n";
		}
		return sa.take_string();
	}
	case H_SUMMARY_TRIGGERS: {
		StringAccum sa;
		for (DTIter qi = td->_summary_triggers.begin(); qi != td->_summary_triggers.end(); qi++) {
			sa << (*qi)->unparse() << "\n";
		}
		return sa.take_string();
	}
	case H_NEIGHBORS: {
		StringAccum sa;
		for (NTIter iter = td->stas.begin(); iter.live(); iter++) {
			DstInfo *nfo = &iter.value();
			sa << nfo->unparse();
		}
		for (NTIter iter = td->aps.begin(); iter.live(); iter++) {
			DstInfo *nfo = &iter.value();
			sa << nfo->unparse();
		}
		return sa.take_string();
	}
    case H_DRP_TRIGGERS: {
		StringAccum sa;
		for (DRPIter qi = td->_drp_triggers.begin(); qi != td->_drp_triggers.end(); qi++) {
			sa << (*qi)->unparse() << "\n";
		}
		return sa.take_string();
    }
	case H_LINKS: {
		StringAccum sa;
		for (CIter iter = td->links.begin(); iter.live(); iter++) {
			CqmLink *nfo = &iter.value();
			sa << nfo->unparse();
		}
		return sa.take_string();
	}
	case H_SIGNAL_OFFSET:
		return String(td->_signal_offset) + "\n";
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerRXStats::write_handler(const String &in_s, Element *e, void *vparam,
		ErrorHandler *errh) {

	EmpowerRXStats *f = (EmpowerRXStats *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_RESET: {
		f->stas.clear();
		f->aps.clear();
		break;
	}
	case H_SIGNAL_OFFSET: {
		int signal_offset;
		if (!IntArg().parse(s, signal_offset))
			return errh->error("signal _offset parameter must be integer");
		f->_signal_offset = signal_offset;
		break;
	}
	case H_DEBUG: {
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerRXStats::add_handlers() {
	add_read_handler("neighbors", read_handler, (void *) H_NEIGHBORS);
	add_read_handler("links", read_handler, (void *) H_LINKS);
	add_read_handler("summary_triggers", read_handler, (void *) H_SUMMARY_TRIGGERS);
	add_read_handler("matches", read_handler, (void *) H_MATCHES);
	add_read_handler("rssi_triggers", read_handler, (void *) H_RSSI_TRIGGERS);
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("signal_offset", read_handler, (void *) H_SIGNAL_OFFSET);
	add_write_handler("signal_offset", write_handler, (void *) H_SIGNAL_OFFSET);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
    add_read_handler("drp_triggers",read_handler,(void*)H_DRP_TRIGGERS);    //DRP Stuff
}

EXPORT_ELEMENT(EmpowerRXStats)
ELEMENT_REQUIRES(bitrate DstInfo Trigger SummaryTrigger RssiTrigger DrpTrigger)
CLICK_ENDDECLS

