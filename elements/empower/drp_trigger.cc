/*
 * rssi_trigger.{cc,hh}
 * Roberto Riggio
 *
 * Copyright (c) 2009 CREATE-NET
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
#include "empowerrxstats.hh"
#include "empowerlvapmanager.hh"
#include "trigger.hh"
#include "drp_trigger.hh"
CLICK_DECLS

DrpTrigger::DrpTrigger(EmpowerLVAPManager * el, EmpowerRXStats * ers) {

}

DrpTrigger::~DrpTrigger() {
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DrpTrigger)

