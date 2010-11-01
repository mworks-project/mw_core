/*
 *  OpenALContextManager.cpp
 *  MWorksCore
 *
 *  Created by David Cox on 10/13/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include "OpenALContextManager.h"
using namespace mw;

namespace mw {
	OpenALContextManager *GlobalOpenALContextManager;
}

OpenALContextManager::OpenALContextManager(){
	
	default_device = shared_ptr<ALDevice>(new ALDevice());

	devices.push_back(default_device);
	

	default_context = shared_ptr<ALContext>(new ALContext(default_device, NULL));
	contexts.push_back(default_context);
	
	setDefaultContextCurrent();
}

OpenALContextManager::~OpenALContextManager(){ }

shared_ptr<ALContext> OpenALContextManager::getDefaultContext(){
	return default_context;
}

void OpenALContextManager::setCurrent(int i){
	
	if(i < 0 || i > contexts.size()){
        throw SimpleException("Attempt to set an OpenAL context current with index out of bounds");
    }
    
    contexts[i]->setCurrent();
}

void OpenALContextManager::setDefaultContextCurrent(){
	
    default_context->setCurrent();
}
