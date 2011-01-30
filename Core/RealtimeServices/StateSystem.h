// * Paul Jankunas on 1/24/06 - Adding virtual destructor.
// Paul Jankunas on 1/31/06 - Added methods to send events when the system
// starts, stops pauses and unpauses use these in your plugins.
#ifndef STATE_SYSTEM_H
#define STATE_SYSTEM_H

#include "MWorksTypes.h"
#include "States.h"
#include "Clock.h"
#include <vector>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>


namespace mw {
class StateSystem : public mw::Component, public enable_shared_from_this<StateSystem>{ //, public RegisteredSingleton<StateSystem> {


public:

    enum action {
        START,
        STOP,
        PAUSE
    };

protected:
	shared_ptr <Clock> the_clock;
	
    
    typedef vector< boost::function<void()> > CallbackList;
    typedef map< StateSystem::action, shared_ptr<CallbackList> >  CallbackTable;
    CallbackTable callback_lists;
        
    
public:
    
	StateSystem(const shared_ptr <Clock> &a_clock);
	
	virtual ~StateSystem();
	
	virtual void start();
	virtual void stop();
	virtual void pause();
	
	virtual bool isRunning();
	virtual bool isPaused();
	
	virtual bool isInAction();
	virtual bool isInTransition();
	
	virtual void setInAction(bool);
	virtual void setInTransition(bool);
	
	// use these to send the proper events at the proper times
	virtual void sendSystemStateEvent();
	
	weak_ptr<State> getCurrentState();
	void setCurrentState(weak_ptr<State> current);
	shared_ptr<Clock> getClock() const;
  
  
    // hooks for state-system-related callbacks
    virtual void registerCallback(StateSystem::action _action, boost::function<void()> callback){ 
        shared_ptr<CallbackList> callback_list = callback_lists[_action];
        if(callback_list != NULL){
            callback_list->push_back(callback);
        }
    };
    
    virtual void serviceCallbacks(action _action){
        shared_ptr<CallbackList> callback_list = callback_lists[_action];
        if(callback_list != NULL){
            
            CallbackList::iterator i; 
            for( i = callback_list->begin(); i != callback_list->end(); ++i){
                (*i)(); // call it
            }
        }
    }
    
    // unregister all callbacks
    virtual void unregisterCallbacks(){
        CallbackTable::iterator i;
        for(i = callback_lists.begin(); i != callback_lists.end(); ++i){
            ((*i).second)->clear();
        }
    }
        
  
    REGISTERED_SINGLETON_CODE_INJECTION(StateSystem)
};

}


#endif

