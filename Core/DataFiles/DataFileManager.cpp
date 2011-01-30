/**
 * DataFileManager.cpp
 *
 * History:
 * David Cox on Wed Oct 06 2004 - Created.
 * Paul Jankunas on 09/06/05 - Added initialzation code for filename
 *  and copying of filename to the open file routine.
 *
 * Copyright (c) 2004 MIT. All rights reserved.
 */

#include "Experiment.h"
#include "Event.h"
#include "DataFileManager.h"
#include "Utilities.h"
#include "ScarabServices.h"
#include <string>
#include "SystemEventFactory.h"
#include "EventBuffer.h"
#include "PlatformDependentServices.h"
#include "boost/filesystem/convenience.hpp"
using namespace mw;

namespace mw {
	SINGLETON_INSTANCE_STATIC_DECLARATION(DataFileManager)
}

DataFileManager::DataFileManager() {
	
    scarab_connection = shared_ptr<ScarabWriteConnection>();
    //session = NULL;
    file_open = false;
    tmp_file = false;
}

DataFileManager::~DataFileManager() { }

int DataFileManager::openFile(const Datum &oeDatum) {
	std::string dFile(oeDatum.getElement(DATA_FILE_FILENAME).getString());
    DataFileOptions opt = (DataFileOptions)oeDatum.getElement(DATA_FILE_OPTIONS).getInteger();
	
	if(dFile.size() == 0) {
		merror(M_FILE_MESSAGE_DOMAIN, 
			   "Attempt to open an empty data file");
		return -1;
	}
    
	if(isFileOpen()) {
		mwarning(M_FILE_MESSAGE_DOMAIN,
				 "Data file already open at %s", 
				 (getFilename()).c_str());
		return -1;
	}
	
	return openFile(dFile, opt, false);
}

int DataFileManager::openFile(std::string _filename, DataFileOptions opt, bool temporary) {

    tmp_file = temporary;
    
    // first we need to format the file name with the correct path and
    // extension
	std::string _ext_(appendDataFileExtension(
											  prependDataFilePath(_filename.c_str()).string()));    
    filename = _ext_;
    
    // form the scarab uri
    std::string prefix = "ldobinary:file://";
    std::string uri = prefix + filename;
	
	if(opt == M_NO_OVERWRITE) {
		FILE *fd = fopen(filename.c_str(), "r");
		if(fd != 0) {
			// badness....don't overwrite my file!
			fclose(fd);
			
			merror(M_FILE_MESSAGE_DOMAIN,
				   "Can't overwrite existing file: %s", filename.c_str());
			
            global_outgoing_event_buffer->putEvent(SystemEventFactory::dataFileOpenedResponse(filename.c_str(), 
																				M_COMMAND_FAILURE));
			return -1;
		}
	}			
	
    // Ensure that the data file directory exists
    boost::filesystem::create_directories(dataFilePath());
    
    if(scarab_create_file(filename.c_str()) != 0){
		merror(M_FILE_MESSAGE_DOMAIN,
			   "Could not create file: %s", filename.c_str());
		return -1;
	}
    scarab_connection = shared_ptr<ScarabWriteConnection>(new ScarabWriteConnection(global_outgoing_event_buffer, uri));
    scarab_connection->connect();
	
    if(scarab_connection->isConnected()) {
        scarab_connection->startThread(M_DATAFILE_SERVICE_INTERVAL_US);
        file_open = true;
		
        // write out the event-code to name/description mapping
        // this is an essential part of the self-describing nature of the
        // MWorks/Scarab format
		global_outgoing_event_buffer->putEvent(SystemEventFactory::componentCodecPackage());
		global_outgoing_event_buffer->putEvent(SystemEventFactory::codecPackage());
		global_outgoing_event_buffer->putEvent(SystemEventFactory::currentExperimentState());
		global_variable_registry->announceAll();
    } else {
        merror(M_FILE_MESSAGE_DOMAIN,
			   "Failed to open file: %s", uri.c_str());
        global_outgoing_event_buffer->putEvent(SystemEventFactory::dataFileOpenedResponse(filename.c_str(), 
																			M_COMMAND_FAILURE));
        return -1;
    }
    
	mprintf(M_FILE_MESSAGE_DOMAIN, "Opening data file: %s", filename.c_str());
	
    // everything went ok so issue the success event
    global_outgoing_event_buffer->putEvent(SystemEventFactory::dataFileOpenedResponse(filename.c_str(), 
																		M_COMMAND_SUCCESS)); 
    return 0;
}

int DataFileManager::openTmpFile(){
    
    if(isFileOpen()){
        return -1;
    }
    
    // create the tmp dir if it is not already there
    boost::filesystem::path tmp_path(prependDataFilePath("/tmp"));
    
    try {
        boost::filesystem::create_directory(tmp_path);
    } catch (boost::filesystem::basic_filesystem_error<boost::filesystem::path>& e){
        fprintf(stderr, e.what()); fflush(stderr);
    }
    
    string tmp_template("/tmp/mw_temp_file_XXXXXX.mwk");
    char *tmp = new char(tmp_template.size() + 1);
    strncpy(tmp, tmp_template.c_str(), tmp_template.size());
    //tmp_template.copy(tmp, tmp_template.size());
    tmp[tmp_template.size()] = '\0';
    int ret_code = mkstemps(tmp, 4);
    
    if(ret_code == EINVAL){
        throw SimpleException("Unable to open temporary file: invalid temp file template");
    }

    if(ret_code == EEXIST){
        throw SimpleException("Unable to open temporary file: cannot create file");
    }
    
    
    string tmp_filename(tmp);
    delete [] tmp;
    
    return openFile(tmp_filename, M_NO_OVERWRITE, true);
}
 
    
int DataFileManager::closeFileIfTmp(){
    if(tmp_file){
        return closeFile();
    }
    
    return 0;
}

int DataFileManager::closeFile() {
    if(file_open) {
        scarab_connection->disconnect();
        // delete scarab_connection;
        file_open = false;
		
		mprintf(M_FILE_MESSAGE_DOMAIN, "Closing data file: %s", filename.c_str());
        global_outgoing_event_buffer->putEvent(SystemEventFactory::dataFileClosedResponse(filename.c_str(), 
																			M_COMMAND_SUCCESS)); 
    } else {
		merror(M_FILE_MESSAGE_DOMAIN,
			   "Attempt to close a data file when there isn't one open");		
	}
    
    tmp_file = false;
    return 0;
}

bool DataFileManager::isFileOpen() {
    return file_open;
}

std::string DataFileManager::getFilename() { 
    return filename;
}

