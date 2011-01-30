/**
 * DataFileManager.h
 *
 * Discussion: The DataFileManager object takes care of opening a data
 *             file and streaming data from the event stream to it.
 *             It follows the "RegisteredSingleton" convention for
 *             instantiation, lifecycle, etc.
 */

#ifndef DATA_FILE_MANAGER_H__
#define DATA_FILE_MANAGER_H__

#include "ScarabServices.h"
#include "ScarabWriteConnection.h"
#include "Event.h"
#include "SystemEventFactory.h"

#define DATA_FILE_FILENAME	"file"
#define DATA_FILE_OPTIONS	"options"

#define M_DATAFILE_SERVICE_INTERVAL_US	20000
namespace mw {
	class DataFileManager : public Component {
		
	private:
		
		std::string filename;
		bool file_open;
        bool tmp_file;
		
		shared_ptr <ScarabWriteConnection> scarab_connection;
		

	public:
		
        DataFileManager();
        ~DataFileManager();
        
        /*!
         * @function openFile
         * @discussion TODO.... issues a M_DATA_FILE_OPENED event
         */
        // open a file from a datastream event.  TODO: non-uniform
		int openFile(const Datum &openFileDatum);
        // open a temporary file (via autosave hooks)
		int openTmpFile();                        

        // the central openFile method
		int openFile(std::string filename, DataFileOptions opt, bool temporary = false);

        /*!
         * @function closeFile
         * @discussion TODO.... issues a M_DATA_FILE_CLOSED event
         */
		int closeFile();
        int closeFileIfTmp();
		
		bool isFileOpen();
        bool isTmpFileOpen(){
            return tmp_file;
        }
		std::string getFilename();
		
		int serviceBuffer();
		int serviceBufferPeriodically();
		
        REGISTERED_SINGLETON_CODE_INJECTION(DataFileManager)
            
	};
	
}
#endif