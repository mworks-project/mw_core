/*
 *  StopDynamicStimulus.h
 *  DynamicStimulusPlugins
 *
 *  Created by bkennedy on 8/17/08.
 *  Copyright 2008 MIT. All rights reserved.
 *
 */

#ifndef STOP_DYNAMIC_STIMULUS_ACTION_H_
#define STOP_DYNAMIC_STIMULUS_ACTION_H_

#include "TrialBuildingBlocks.h"
#include "DynamicStimulusDriver.h"

class StopDynamicStimulus : public Action {	
protected:
	shared_ptr<DynamicStimulusDriver> dynamic_stimulus;
public:
	StopDynamicStimulus(shared_ptr<DynamicStimulusDriver> the_dynamic_stimulus);
	virtual bool execute();
};

#endif 
// STOP_DYNAMIC_STIMULUS_ACTION_H_
