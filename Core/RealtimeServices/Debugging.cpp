/*
 *  Debugging.cpp
 *  MWorksCore
 *
 *  Created by David Cox on 2/11/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "Debugging.h"


#include "Clock.h"
#include "StandardVariables.h"
using namespace mw;

namespace mw {
	bool debugger_enabled;
	
	
	// Stall or proceed according to the settings of the debugging variables
	void debuggerCheck(){
		
		if(!StandardVariables::debuggerRunning->getValue()){
			
			while(!StandardVariables::debuggerRunning->getValue()){
				if((long)(StandardVariables::debuggerStep->getValue()) > 0){
					StandardVariables::debuggerStep->setValue((long)StandardVariables::debuggerStep->getValue() - 1);
					break;
				}
				shared_ptr <Clock> clock = Clock::instance();
				clock->sleepMS(50);
			}
		}
	}
}
