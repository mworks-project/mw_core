/*
 *  VariableSave.cpp
 *  MWorksCore
 *
 *  Created by bkennedy on 8/4/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include <string>
#include "VariableSave.h"
#include "Experiment.h"
#include "GenericData.h"
#include "PlatformDependentServices.h"
#include "XMLVariableWriter.h"

using namespace mw;

bool VariableSave::saveExperimentwideVariables(const boost::filesystem::path &file) {
	using namespace std;

    vector< shared_ptr<Variable> > variable_list = global_variable_registry->getGlobalVariables();
    vector< shared_ptr<Variable> > variables_to_write;
    
	vector< shared_ptr<Variable> >::iterator i;
    
	for(i = variable_list.begin(); i != variable_list.end(); i++)
	{
		shared_ptr<Variable> var = *i;
		VariableProperties *interface = var->getProperties();
		
		if(interface->getPersistant()){
            variables_to_write.push_back(var);
        }
	}
    
    try {
        XMLVariableWriter::writeVariablesToFile(variables_to_write, file);
    } catch (std::exception& e){
        merror(M_SYSTEM_MESSAGE_DOMAIN, "Error writing variables to file");
        return false;
    }
	return true;
}