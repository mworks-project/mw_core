/*
 *  ScopedVariableEnvironment.cpp
 *  MWorksCore
 *
 *  Created by David Cox on 3/26/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "ScopedVariableEnvironment.h"
#include "ScopedVariable.h"
#include "VariableProperties.h"
using namespace mw;

ScopedVariableEnvironment::ScopedVariableEnvironment() : variables(){
	//current_context = NULL;
}

// Add a new scoped variable to this environment
int ScopedVariableEnvironment::addVariable(shared_ptr<ScopedVariable> var){
	variables.push_back(var);
	int index = variables.size() - 1;
    var->setContextIndex(index);
	var->setEnvironment(this);
	
	return index;
}

// Generate a new context that is valid in this environment
// Should be called *after* all variables are added to the environment
shared_ptr<ScopedVariableContext> ScopedVariableEnvironment::createNewContext(){
	shared_ptr<ScopedVariableContext> new_context( 
						new ScopedVariableContext(this));
	
	return new_context;
}

shared_ptr<ScopedVariableContext> ScopedVariableEnvironment::createNewDefaultContext(){

	shared_ptr<ScopedVariableContext> new_context = createNewContext();
	
    vector< shared_ptr<ScopedVariable> >::iterator i;
	for(i = variables.begin(); i != variables.end(); ++i){
		shared_ptr<ScopedVariable> var = *i;
		VariableProperties *props = var->getProperties();
		new_context->set(var->getContextIndex(), props->getDefaultValue());
	}
	
	return new_context;
}


// Set the current variable context
// Should have been created by the createNewContext method
void ScopedVariableEnvironment::setCurrentContext(shared_ptr<ScopedVariableContext> new_context){
	current_context = new_context;
}

// ScopedVariable delegate methods
Datum ScopedVariableEnvironment::getValue(int index){
	if(current_context != NULL){
		return current_context->get(index);
	}
	
	// TODO: warn
	return Datum();
}


void ScopedVariableEnvironment::setValue(int index, Datum value){
	if(current_context != NULL){
		current_context->set(index, value);
	} else {
		// TODO: warn
		merror(M_PARADIGM_MESSAGE_DOMAIN, "Attempt to set a value on a missing context");
		
	}
}


void ScopedVariableEnvironment::announceAll(){
    
    vector< shared_ptr<ScopedVariable> >::iterator i;
	for(i = variables.begin(); i != variables.end(); ++i){
		shared_ptr<ScopedVariable> var = *i;
		if(var != shared_ptr<ScopedVariable>()){
			var->announce();
			//TODO Maybe this needs to be here?
			//var->performNotifications();

		}
	}
}
