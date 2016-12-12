/*
 */
#include <click/config.h>
#include "empowerrxstats.hh"
#include "empowerlvapmanager.hh"
#include "trigger.hh"
#include "drp_trigger.hh"
CLICK_DECLS

DrpTrigger::DrpTrigger(EtherAddress eth1,EtherAddress eth2, uint32_t trigger_id, uint16_t period,String rule, bool dispatched ,EmpowerLVAPManager * el,
        EmpowerRXStats * ers) : Trigger(eth1, trigger_id, period, el, ers), _dispatched(dispatched) , _val(11),_rule(rule),_eth1(eth1),_eth2(eth2) {
}

DrpTrigger::~DrpTrigger() {
}

String DrpTrigger::unparse() {
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

