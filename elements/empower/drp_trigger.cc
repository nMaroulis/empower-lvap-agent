/*
 */
#include <click/config.h>
#include "empowerrxstats.hh"
#include "empowerlvapmanager.hh"
#include "trigger.hh"
#include "drp_trigger.hh"
CLICK_DECLS

DrpTrigger::DrpTrigger(EtherAddress eth, uint32_t trigger_id, uint16_t period, char * rule, bool dispatched ,EmpowerLVAPManager * el,
        EmpowerRXStats * ers) : Trigger(eth, trigger_id, period, el, ers), _dispatched(dispatched) , _val(11) {
    if (_debug) { click_chatter("DRP :: %{element} :: %s , rule ( )", this, __func__, rule); }
}

DrpTrigger::~DrpTrigger() {
    if (_debug) { click_chatter("DRP :: %{element} :: %s", this, __func__); }
}

String DrpTrigger::unparse() {
    if (_debug) { click_chatter("DRP :: %{element} :: %s", this, __func__); }
    StringAccum sa;
    sa << Trigger::unparse();
    sa << " addr ";
    sa << _eth.unparse();
    sa << " val " << _val;
    if (_dispatched) {
        sa << " dispatched";
    }
    return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DrpTrigger)

