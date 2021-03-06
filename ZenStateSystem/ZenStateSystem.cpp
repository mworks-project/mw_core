/*
 *  ZenStateSystem.cpp
 *  MWorksCore
 *
 *  Created by David Cox on 6/16/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */


#include "MWorksCore/Experiment.h"
#include "ZenStateSystem.h"
#include "StateSystem.h"
#include "MWorksCore/Utilities.h"
#include "MWorksCore/Scheduler.h"
#include "MWorksCore/StateSystem.h"
#include "MWorksCore/StandardVariables.h"
#include "MWorksCore/Debugging.h"



#ifdef __APPLE__
// this must go AFTER the mw includes
#import <Cocoa/Cocoa.h>
#endif

#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/thread_policy.h>
#include <mach/task_policy.h>
#include <mach/thread_act.h>
#include <sys/sysctl.h>

#include "MachUtilities.h"

#ifdef	LOW_PRIORITY_MODE
	#define STATE_SYSTEM_PRIORITY	0
#else
	#define STATE_SYSTEM_PRIORITY 96
#endif

using namespace mw;
using namespace boost;

pthread_mutex_t state_system_mutex;
pthread_t state_system_thread;

static bool in_action, in_transition, is_running, is_paused;
static weak_ptr<State> current_state;


void *checkStateSystem(void *noargs);

    
StandardStateSystem::StandardStateSystem(const shared_ptr <Clock> &a_clock) : StateSystem(a_clock) {
    is_running = false;
    is_paused = false;
}

void StandardStateSystem::start(){

    //E->setInt(taskMode_edit, RUNNING);

//	(*state_system_mode) = RUNNING;
  
	mprintf("Called start on state system");
	
	if(GlobalCurrentExperiment == NULL){
		merror(M_STATE_SYSTEM_MESSAGE_DOMAIN,
			  "Cannot start state system without a valid experiment defined");
		return;
	}
	
	// TODO: is this implicit cast kosher?
	weak_ptr<State> exp_ref(GlobalCurrentExperiment);
	GlobalCurrentExperiment->setCurrentState(exp_ref);
	//GlobalCurrentExperiment->setCurrentState(GlobalCurrentExperiment->getCurrentProtocol());
		
	shared_ptr <StateSystem> *shared_ptr_to_this_ptr = 
            new shared_ptr<StateSystem>(component_shared_from_this<StateSystem>());
	pthread_create(&state_system_thread, NULL, checkStateSystem, shared_ptr_to_this_ptr);
    
    struct sched_param sp;
    memset(&sp, 0, sizeof(struct sched_param));
    sp.sched_priority=STATE_SYSTEM_PRIORITY;
    
    pthread_setschedparam(state_system_thread, SCHED_RR, &sp);
    
    is_running = true;
	sendSystemStateEvent();
	
}
    
    
void StandardStateSystem::stop(){
    // stop this thing somehow....

	if(state_system_mode != NULL){
		// is_running = false;
		(*state_system_mode) = IDLE;
	}
	
	
	// TODO: need to stop ongoing schedules...
	// esp. IO devices
    sendSystemStateEvent();
}

void StandardStateSystem::pause(){
    // send in a message to suspend the state system
//    sendSystemPausedEvent();
    //is_paused = true;
}

bool StandardStateSystem::isRunning(){
    return is_running;
}

bool StandardStateSystem::isPaused(){
    return is_paused;
}

bool StandardStateSystem::isInAction(){
    return in_action;
}

bool StandardStateSystem::isInTransition(){
    return in_transition;
}

void StandardStateSystem::setInAction(bool isit){
    in_action = isit;
}

void StandardStateSystem::setInTransition(bool isit){
    in_transition = isit;
}

weak_ptr<State> StandardStateSystem::getCurrentState(){
    return current_state;
}

void StandardStateSystem::setCurrentState(weak_ptr<State> newcurrent){
    current_state = newcurrent;
}

//  check state system
void *checkStateSystem(void *void_state_system){

#ifdef __APPLE__
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
#endif
    
	// Hand-off the self state system reference
	shared_ptr<StateSystem> *ss_ptr = (shared_ptr<StateSystem> *)void_state_system;
	shared_ptr<StateSystem> ss = *ss_ptr;
	delete ss_ptr;

	
	weak_ptr<State> next_state;


	int bus_speed, mib [2] = { CTL_HW, HW_BUS_FREQ };
	size_t len;   
	len = sizeof (bus_speed);
	sysctl (mib, 2, &bus_speed, &len, NULL, 0);
	
	int didit = set_realtime(STATE_SYSTEM_PRIORITY);
	
	if(didit){
		//fprintf(stderr,"Scheduler went realtime.  Rock on.\n");
		//fflush(stderr);
	} else {
		fprintf(stderr,"Scheduler didn't go realtime.  Bummer.\n");
		fflush(stderr);
	}


	mprintf("Starting state system....");


	if(GlobalCurrentExperiment == NULL){
		merror(M_STATE_SYSTEM_MESSAGE_DOMAIN,
				"GlobalCurrentExperiment is not defined.");
		return NULL;
	}
    //mprintf("----------setting task  mode to running------------");
	(*state_system_mode) = (long) RUNNING;
	current_state = GlobalCurrentExperiment->getCurrentState();
    
	//mExperiment *testexp = GlobalCurrentExperiment;
	
	if(current_state.expired()){
		merror(M_STATE_SYSTEM_MESSAGE_DOMAIN,
				"current state is NULL. Shutting down state system...");
				
		(*state_system_mode) = (long)IDLE;
	}
        
        //while(1){
		
	if(current_state.expired() == true){
		// TODO: better throw
		throw SimpleException("Invalid current state within state system");
	}
	
	shared_ptr<State> current_state_shared(current_state);
	
	while((current_state_shared != NULL			  &&		// broken system
	      (long)(*state_system_mode) != IDLE      &&			// hard stop
		  (long)(*state_system_mode) != STOPPING)  ||		// stop requested
								!(current_state_shared->isInterruptible())){ // might not be
																			   // an okay place to stop
		
		// this is a global bool; it is set a by a listener to the variable
		// debuggerActive
		if(debugger_enabled){
			debuggerCheck();
		}
		
		
		
		//mprintf("State system main loop, current state = %d", current_state);
		if(!in_action){
			in_action = true;
			
			//mState *test_state = current_state;
			
			try {
				current_state_shared->action();
			} catch(std::exception& e){
				merror(M_PARADIGM_MESSAGE_DOMAIN,
					   "Stopping state system: %s", e.what());
				state_system_mode->setValue((long)STOPPING);
				break;
			}
		
			// finished performing action
			in_action = false;
		}
		
		if(!in_transition){
			in_transition = true;

		
			try {
				next_state = current_state_shared->next();
			} catch (std::exception& e){
				merror(M_PARADIGM_MESSAGE_DOMAIN,
					  "Stopping state system: %s", e.what());
				state_system_mode->setValue((long)STOPPING);
				break;
			}
			
			while(next_state.expired()){// && E->getInt(taskMode_edit) == RUNNING){
				
				if(current_state_shared->isInterruptible() &&
					((long)(*state_system_mode) == IDLE  ||			// hard stop
					 (long)(*state_system_mode) == STOPPING)){
					 next_state = GlobalCurrentExperiment;
					 break;
				}
				
				// no next state yet, sleep until the next tick
				ss->getClock()->sleepUS(500);
				try {
					next_state = current_state_shared->next();
				} catch (std::exception& e){
					merror(M_PARADIGM_MESSAGE_DOMAIN,
						  "Stopping state system: %s", e.what());
                    state_system_mode->setValue((long)STOPPING);
                    break;
				}
				
			}
			
            shared_ptr<State> next_state_shared;
            
            try{
                shared_ptr<State> attempt(next_state); // cast weak_ptr into shared_ptr
                next_state_shared = attempt; // machination required because of weak to shared conversion semantics
			} catch (std::exception& e){
                mwarning(M_STATE_SYSTEM_MESSAGE_DOMAIN, "Failed to acquire shared_ptr from next_state; coming to an abrupt halt");
                (*state_system_mode) = IDLE;
                continue;
            }
			
			//mprintf("State system moving on... %d", next_state);
			
			GlobalCurrentExperiment->setCurrentState(next_state);
			
			
			
			// If we've finished
			if(current_state_shared.get() == GlobalCurrentExperiment.get() && 
					next_state_shared.get() == GlobalCurrentExperiment.get()){
					mprintf("Returned to Experiment node, halting state system...");
					(*state_system_mode) = IDLE;
					current_state = weak_ptr<State>();
					next_state = weak_ptr<State>();
					continue;
			}
			
			
			current_state = next_state;
			current_state_shared = shared_ptr<State>(current_state);
			
			next_state = weak_ptr<State>();
			next_state_shared = shared_ptr<State>();
			
			// finished transition
			in_transition = false;
		}
	}
	
	
	in_action = false;
	in_transition = false;
	is_running = false;
    (*state_system_mode) = IDLE;    
	mprintf("State system ending");
	
	
	// DDC: graceful stop?
	if(GlobalCurrentExperiment != NULL){
		mprintf("Reseting experiment");
		GlobalCurrentExperiment->reset();
	}
    
    
#ifdef __APPLE__
    [pool drain];
#endif
    
	
	pthread_exit(0);
	return NULL;
}





