#ifndef OPENAL_CONTEXT_MANAGER_H_
#define OPENAL_CONTEXT_MANAGER_H_

/*
 *  OpenALContextManager.h
 *  MWorksCore
 *
 *  Created by David Cox on 10/13/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include <OpenAL/al.h>
//#include <OpenAL/alc.h>
#include "alc.h" //customized header for gcc4.2

// shifting sands
//#include <OpenAL/alut.h>
#include "alut.h"

#include "Utilities.h"

#include <boost/format.hpp>


namespace mw {

    class ALDevice{
		
        protected:
        
        ALCdevice *device;
        int error;
        
        public:
		
        ALDevice(){
            device = alcOpenDevice(NULL);
            if ((error = alGetError()) != AL_NO_ERROR) { 
                throw SimpleException((boost::format("Failed to open OpenAL sound device (error code %d)") % error).str());
            }
        }
        
		ALDevice(ALCdevice *_device){
			device = _device;
		}
        
        ~ALDevice(){
            alcCloseDevice(device);
            if ((error = alGetError()) != AL_NO_ERROR) { 
                merror(M_SYSTEM_MESSAGE_DOMAIN,
                       "Failed to close OpenAL sound device (error code %d)",
                       error);
            }
        }
		
		operator ALCdevice *(){
			return device;
		}
		
		void operator= (ALCdevice *_device){
			device = _device;
		}
		
		ALCdevice *getRawDevice(){ return device; }
	};
	


	class ALContext{
		
        protected:
        
        ALCcontext *context;
        int error;
        
        public:
		
		ALContext(shared_ptr<ALDevice> device, const ALCint *attrlist){
            context = alcCreateContext(device->getRawDevice(), attrlist);
        }
        
		ALContext(ALCcontext *_context){
			context = _context;
		}
        
        ~ALContext(){
            alcDestroyContext(context);
        }
		
		operator ALCcontext *(){
			return context;
		}
		
		void operator= (ALCcontext *_context){
			context = _context;
		}
        
        void setCurrent(){
            alcMakeContextCurrent(context);
            if ((error = alGetError()) != AL_NO_ERROR) { 
                throw SimpleException((boost::format("Failed to set default OpenAL context current (error code %d)") % error).str());
            }
            
        }
		
		ALCcontext *getRawContext(){ return context; }
	};
	

	
	class OpenALContextManager {
		
	protected:
		
		shared_ptr<ALDevice> default_device;
		vector< shared_ptr<ALDevice> > devices;
		
		shared_ptr<ALContext> default_context;
		vector< shared_ptr<ALContext> > contexts;
		
		ALuint error;
		
	public:
		
		OpenALContextManager();
		~OpenALContextManager();
		
		shared_ptr<ALContext> getDefaultContext();
		void setCurrent(int i);
		void setDefaultContextCurrent();
		
	};
	
	extern OpenALContextManager *GlobalOpenALContextManager;
}
#endif
