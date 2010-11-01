/*
 *  ScheduledActions.cpp
 *  MWorksCore
 *
 *  Created by bkennedy on 11/24/08.
 *  Copyright 2008 mit. All rights reserved.
 *
 */

#include "ScheduledActions.h"
#include "boost/bind.hpp"
using namespace mw;

ScheduledActions::ScheduledActions(shared_ptr<Variable> _n_repeats, 
									 shared_ptr<Variable> _delay, 
									 shared_ptr<Variable> _interval){						  
	n_repeats = _n_repeats;
	delay = _delay;
	interval = _interval;
	setName("ScheduledActions");
}

void ScheduledActions::addAction(shared_ptr<Action> _action){
	action_list.push_back(_action);
}

void ScheduledActions::addChild(std::map<std::string, std::string> parameters,
								 ComponentRegistry *reg,
								 shared_ptr<mw::Component> child){
	
	shared_ptr<Action> act = dynamic_pointer_cast<Action,mw::Component>(child);
	if(act == 0) {
		throw SimpleException("Attempting to add illegal action (" + child->getTag() + ") to scheduled action (" + this->getTag() + ")");
	}
	addAction(act);
	act->setOwner(getSelfPtr<State>());
}

bool ScheduledActions::execute(){
	shared_ptr <Clock> clock = Clock::instance();
	timeScheduled = clock->getCurrentTimeUS();
	nRepeated = 0;
	
	shared_ptr<Scheduler> scheduler = Scheduler::instance();	
	shared_ptr<ScheduledActions> this_one = shared_from_this();
	node = scheduler->scheduleUS(FILELINE, 
								 (MWTime)(*delay),
								 (MWTime)(*interval),
								 (long)(*n_repeats),
								 boost::bind(scheduled_action_runner, this_one),
								 M_DEFAULT_PRIORITY,
								 M_DEFAULT_WARN_SLOP_US,
								 M_DEFAULT_FAIL_SLOP_US,
								 M_MISSED_EXECUTION_CATCH_UP);
	return true;
}

shared_ptr<ScheduleTask> ScheduledActions::getNode(){ 
	return node; 
}

MWTime ScheduledActions::getDelay() const {
	return (MWTime)(*delay);
}

MWTime ScheduledActions::getInterval() const {
	return (MWTime)(*interval);
}

MWTime ScheduledActions::getTimeScheduled() const {
	return timeScheduled;
}

unsigned int ScheduledActions::getNRepeated() const {
	return nRepeated;
}

void ScheduledActions::executeActions() {

    vector< shared_ptr<Action> >::iterator i;
	for(i = action_list.begin(); i != action_list.end(); ++i){
		(*i)->announceEntry();
		(*i)->execute();
		(*i)->announceExit();
	}
	++nRepeated;
}

/****************************************************************
 *                       ScheduledActions Methods
 ****************************************************************/
namespace mw {
	void *scheduled_action_runner(const shared_ptr<ScheduledActions> &sa){
		shared_ptr <Clock> clock = Clock::instance();
		MWTime delta = clock->getCurrentTimeUS() - ((sa->getTimeScheduled() + sa->getDelay()) + sa->getNRepeated()*sa->getInterval());
		
		if(delta > 2000) {
			merror(M_SCHEDULER_MESSAGE_DOMAIN, "Scheduled action is starting %lldus behind intended schedule", delta); 
		} else if (delta > 500) {
			mwarning(M_SCHEDULER_MESSAGE_DOMAIN, "Scheduled action is starting %lldus behind intended schedule", delta); 
		}
		
		sa->executeActions();
		return 0;
	}
}

shared_ptr<mw::Component> ScheduledActionsFactory::createObject(std::map<std::string, std::string> parameters,
															  ComponentRegistry *reg) {
	REQUIRE_ATTRIBUTES(parameters, "delay", "repeats", "duration");
	
	shared_ptr<Variable> delay = reg->getVariable(parameters.find("delay")->second);
	shared_ptr<Variable> duration = reg->getVariable(parameters.find("duration")->second);
	shared_ptr<Variable> repeats = reg->getVariable(parameters.find("repeats")->second);
	
	checkAttribute(duration, parameters["reference_id"], "duration", parameters.find("duration")->second);
	
	checkAttribute(delay, parameters["reference_id"], "delay", parameters.find("delay")->second);
	
	checkAttribute(repeats, parameters["reference_id"], "repeats", parameters.find("repeats")->second);
	
	
	// TODO .. needs more work, the actual actions aren't included here
	
	shared_ptr <mw::Component> newScheduledActions = shared_ptr<mw::Component>(new ScheduledActions(repeats, delay, duration));
	return newScheduledActions;		
}

