////////////////////////////////////////////////////////////////////////////////
//
//  File           : crud_file_io.h
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CRUD storage system.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Mon Oct 20 12:38:05 PDT 2014
//

// Includes
#include <malloc.h>
#include <string.h>

// Project Includes
#include <crud_file_io.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>

// Defines
#define CIO_UNIT_TEST_MAX_WRITE_SIZE 1024
#define CRUD_IO_UNIT_TEST_ITERATIONS 10240

// Other definitions

// Type for UNIT test interface
typedef enum {
	CIO_UNIT_TEST_READ   = 0,
	CIO_UNIT_TEST_WRITE  = 1,
	CIO_UNIT_TEST_APPEND = 2,
	CIO_UNIT_TEST_SEEK   = 3,
} CRUD_UNIT_TEST_TYPE;

// File system Static Data
// This the definition of the file table
CrudFileAllocationType crud_file_table[CRUD_MAX_TOTAL_FILES]; // The file handle table

// Pick up these definitions from the unit test of the crud driver
CrudRequest construct_crud_request(CrudOID oid, CRUD_REQUEST_TYPES req,
		uint32_t length, uint8_t flags, uint8_t res);
int deconstruct_crud_request(CrudRequest request, CrudOID *oid,
		CRUD_REQUEST_TYPES *req, uint32_t *length, uint8_t *flags,
		uint8_t *res);

// Global flag representing the crud interface initialization
uint8_t crudInitialized;

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_format
// Description  : This function formats the crud drive, and adds the file
//                allocation table.
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

uint16_t crud_format(void) {
	// Declaring Variables
	CrudRequest request;
	CrudResponse response; 

	// Generating a CRUD_INIT request and checking for success
	if( crud_init() )
		return -1; // failed to initialize crud interface
	else { // Success...continue

		// Generating a CRUD_FORMAT request... clears the object store
		request = create_crudrequest( 0, CRUD_FORMAT, 0, CRUD_NULL_FLAG );
		response = crud_bus_request( request, NULL );

		// Checking for success
		if ( response & 1 )
			return -1; // failed crud format request
		else {
			// Clearing the crud_file_table.  Each entry in the table has 29 bytes 
			memset( crud_file_table, 0, sizeof( crud_file_table ) );

			// Creating a priority object (saving the table)
			request = create_crudrequest( 0, CRUD_CREATE, sizeof( crud_file_table ), CRUD_PRIORITY_OBJECT );
			response = crud_bus_request( request, crud_file_table );

			// Checking for success
			if( response & 1 )
				return -1; // failed
			else {
				// Log, return successfully
				logMessage(LOG_INFO_LEVEL, "... formatting complete.");
				return(0);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_mount
// Description  : This function mount the current crud file system and loads
//                the file allocation table.
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

uint16_t crud_mount(void) {
	// Declaring Variables
	CrudRequest request;
	CrudResponse response; 

	// Initializing crud interface
	if( !crudInitialized )
		crud_init();

	if( crudInitialized ) {
		// reading the content from the priority object into the local file table
		request = create_crudrequest( 0, CRUD_READ, sizeof( crud_file_table ), CRUD_PRIORITY_OBJECT );
		response = crud_bus_request( request, crud_file_table );

		// Checking for success
		if( response & 1 )
			return -1; // failed
		else {
			// Log, return successfully
			logMessage(LOG_INFO_LEVEL, "... mount complete.");
			return(0);
		}
	} else return -1; // failed, crud did not initialize
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_unmount
// Description  : This function unmounts the current crud file system and
//                saves the file allocation table.
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

uint16_t crud_unmount(void) {
	// Declaring Variables
	CrudRequest request;
	CrudResponse response;

	if( crudInitialized ) {
		// Generating a CRUD_UPDATE request
		request = create_crudrequest( 0, CRUD_UPDATE, sizeof( crud_file_table ), CRUD_PRIORITY_OBJECT );
		response = crud_bus_request( request, crud_file_table );

		// checking for success
		if( response & 1 )
			return -1; // crud update request failed
		else {
			// Generating a CRUD_CLOSE request
			request = create_crudrequest( 0, CRUD_CLOSE, 0, CRUD_NULL_FLAG );
			response = crud_bus_request( request, NULL );

			// Checking for success
			if( response & 1 )
				return -1; // crud close request failed
			else { 
				// Log, return successfully
				logMessage(LOG_INFO_LEVEL, "... unmount complete.");
				return (0);
			}
		}
	} else return -1; // crud interface not initialized
}

// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_crudrequest 
// Description  : This function creates the structure for a crud request
//
// Inputs       : objID (Object ID), reqType (type of Crud Request), length
// Outputs      : Returns a CrudRequest

CrudRequest create_crudrequest( CrudOID objID, CRUD_REQUEST_TYPES reqType, uint32_t length, uint8_t flags ) {
	// Declaring CrudRequest to be constructed and returned
	CrudRequest request; 

	// creating the structure
	request = objID; 
	request <<= 4;       // making room for 4 bit reqType 
	request += reqType; 
	request <<= 24;      // making room for 24 bit length 
	request += length;
	request <<= 3;       // making room for 3 bit flag
	request += flags; 
	request <<= 1;       // 1 bit shift (final bit is success bit)
	
	return request;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_crudresponse
// Description  : Receives the CrudResonse and extracts the OID
//
// Inputs       : CrudResponse
// Outputs      : return 1 if failure, 0 if success

uint8_t extract_crudresponse( CrudResponse response, int16_t fh ) {
	// checking the success value
	if( (response & 1) && (fh >= 0) && (fh <= CRUD_MAX_TOTAL_FILES) )
		return 1; // failed
	else {
 		crud_file_table[fh].object_id = (uint32_t)(response >> 32);
		return 0; // success
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_initialize
// Description  : This function initializes the crud interface
//
// Inputs       : void
// Outputs      : return -1 if failure, 0 if success

uint8_t crud_init( void ) {
	// Generating a request and calling the crud interface
	CrudRequest request = create_crudrequest( 0, CRUD_INIT, 0, 0 );
	CrudResponse response = crud_bus_request( request, NULL );

	// Checking for success
	if( response & 1 )
		return -1; // failed
	else { 
		crudInitialized = 1;
		return 0; // success
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - the path "in the storage array"
// Outputs      : file handle if successful, -1 if failure

int16_t crud_open(char *path) {
	// Initializing variables
	CrudRequest request;                 // new crud request
	CrudResponse response;               // new crud response
	int i, index = CRUD_MAX_TOTAL_FILES; // for loop interator; index if file path not found in table
	uint8_t found = 0;                   // flag determining if the file path was found in the file table

	// Initializing CRUD interface
	if( !crudInitialized )
		crud_init();
 	
	if( crudInitialized ) {
		// Searching the file table.  If the file path is already there, set open flag to 1, else allocate a spot in the file table.
		for( i = 0; i < CRUD_MAX_TOTAL_FILES; i++ ) {
			if( !strcmp( path, crud_file_table[i].filename ) ) {
				crud_file_table[i].open = 1;
				found = 1;
				break; // once found, exit loop.
			} 
			else if( ( i < index ) && ( !strcmp( crud_file_table[i].filename, "" ) ) )
				index = i;
		}

		// File not in table.  Make entry
		if( !found ) {
			request = create_crudrequest( 0, CRUD_CREATE, 0, 0 ); // crud create request
			response = crud_bus_request( request, NULL );      // sending request
			if( !extract_crudresponse( response, index ) ) {
				strcpy( crud_file_table[index].filename, path );
				crud_file_table[index].position = 0;
				crud_file_table[index].length = 0;
				crud_file_table[index].open = 1;
				return index;
			} else return -1; // return failed
		} else return i; // successfull, returning fd
	} else return -1; // crud not initialized. Failed.
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_close
// Description  : This function closes the file
//
// Inputs       : fd - the file handle of the object to close
// Outputs      : 0 if successful, -1 if failure

int16_t crud_close(int16_t fd) {
	// checking parameters
	if( fd >= 0 && fd <= CRUD_MAX_TOTAL_FILES && crud_file_table[fd].open ) {
		crud_file_table[fd].open = 0;
		crud_file_table[fd].position = 0;
		return 0;
	} else return -1;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_read
// Description  : Reads up to "count" bytes from the file handle "fd" into the
//                buffer  "buf".
//
// Inputs       : fd - the file descriptor for the read
//                buf - the buffer to place the bytes into
//                count - the number of bytes to read
// Outputs      : the number of bytes read or -1 if failures

int32_t crud_read(int16_t fd, void *buf, int32_t count) {
	// Declaring and Initializing variables
	CrudRequest request;
	CrudResponse response;
	int32_t readBytes = 0; // determines the number of bytes to read and also the retval
	char *tempBuf = (char*)malloc(crud_file_table[fd].length); // temp buffer to hold read data from object


	// verifying the crud interface is initialized, fd is valid, and the file is open
	if( crudInitialized && fd >= 0 && fd <= CRUD_MAX_TOTAL_FILES && crud_file_table[fd].open ) {

		// determining the number of bytes to read
		if( (crud_file_table[fd].position + count) <= crud_file_table[fd].length )   // can read count bytes
			readBytes = count;
		else // reading count bytes continues past LENGTH
			readBytes = crud_file_table[fd].length - crud_file_table[fd].position;

		// generating a crud request and sending the request
		request = create_crudrequest( crud_file_table[fd].object_id, CRUD_READ, crud_file_table[fd].length, 0 );
		response = crud_bus_request( request, tempBuf );

		// copying correct bytes into buf
		memcpy( buf, &tempBuf[crud_file_table[fd].position], readBytes );

		// checking result of response
		if( !(response & 1) ) {
			crud_file_table[fd].position += readBytes;
			free(tempBuf); // freeing malloced memory
			return readBytes;
		}
		else {
			free(tempBuf);
			return -1;
		}
	} else {
		// freeing memory allocation
		free(tempBuf);
		tempBuf = NULL;
		return -1;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_write
// Description  : Writes "count" bytes to the file handle "fd" from the
//                buffer  "buf"
//
// Inputs       : fd - the file descriptor for the file to write to
//                buf - the buffer to write
//                count - the number of bytes to write
// Outputs      : the number of bytes written or -1 if failure

int32_t crud_write(int16_t fd, void *buf, int32_t count) {
	// Declaring and Initializing variables
	CrudRequest request;
	CrudResponse response;
	uint32_t oldPos;                                  // holds the old fd position
	char oldData[crud_file_table[fd].length];         // creating a buffer to store old data
	char newData[crud_file_table[fd].length + count]; // creating a buffer for new data

	// Checking crud interface initialized, valid fd, and the file is open
	if( crudInitialized && fd >= 0 && fd <= CRUD_MAX_TOTAL_FILES && crud_file_table[fd].open ) {

		// extracting old object data, saving, deleting, and creating a new object
		oldPos = crud_file_table[fd].position;                  // storing the value of POS
		crud_file_table[fd].position = 0;                       // updating position to read from the beginning
		crud_read( fd, oldData, crud_file_table[fd].length );   // old data extracted and stored in "oldData"
		memcpy( newData, oldData, crud_file_table[fd].length ); // saving old data into new data
		memcpy( &newData[oldPos], buf, count );                 // saving write data into new buffer

		// number of bytes to write exceeds LENGTH, must delete current object and create a new one
		if( oldPos + count > crud_file_table[fd].length ) {

			// Creating request to delete object.  If response is successful, create new object with new length 
			request = create_crudrequest( crud_file_table[fd].object_id, CRUD_DELETE, 0, 0 );

			if( !( crud_bus_request( request, newData ) & 1 ) ) {
				// Creating request to create a new object
				request = create_crudrequest( 0, CRUD_CREATE, oldPos + count, 0 ); // creating request
				response = crud_bus_request( request, newData ); // sending request
				
				// checking response and updating table entry if successful
				if( !extract_crudresponse( response, fd ) ) {
					crud_file_table[fd].position = oldPos + count; 
					crud_file_table[fd].length = oldPos + count; 
					return count;
				} else return -1; // crud bus request failed
			} else return -1; // crud bus request failed
		}
		// number of bytes to write does not exceed LENGTH, must update object
		else {
			// creating new update request and sending request
			request = create_crudrequest( crud_file_table[fd].object_id, CRUD_UPDATE, crud_file_table[fd].length, 0 );
			response = crud_bus_request( request, newData ); 
			
			// checking response and updating new position if successful
			if( !extract_crudresponse( response, fd )) {
				crud_file_table[fd].position = oldPos + count; 
				return count;
			} else return -1; // crud bus request failed
		}
	} else return -1;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : crud_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - the file descriptor for the file to seek
//                loc - offset from beginning of file to seek to
// Outputs      : 0 if successful or -1 if failure

int32_t crud_seek(int16_t fd, uint32_t loc) {
	// Checking crud interface initialized and fd is valid
	if( crudInitialized && fd >= 0 && fd <= CRUD_MAX_TOTAL_FILES ) {
		// checking boundary conditions of loc
		if( loc <= crud_file_table[fd].length ) {
			crud_file_table[fd].position = loc;
			return 0;
		} else return -1;
	} else return -1;
}

// Module local methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : crudIOUnitTest
// Description  : Perform a test of the CRUD IO implementation
//
// Inputs       : None
// Outputs      : 0 if successful or -1 if failure

int crudIOUnitTest(void) {

	// Local variables
	uint8_t ch;
	int16_t fh, i;
	int32_t cio_utest_length, cio_utest_position, count, bytes, expected;
	char *cio_utest_buffer, *tbuf;
	CRUD_UNIT_TEST_TYPE cmd;
	char lstr[1024];

	// Setup some operating buffers, zero out the mirrored file contents
	cio_utest_buffer = malloc(CRUD_MAX_OBJECT_SIZE);
	tbuf = malloc(CRUD_MAX_OBJECT_SIZE);
	memset(cio_utest_buffer, 0x0, CRUD_MAX_OBJECT_SIZE);
	cio_utest_length = 0;
	cio_utest_position = 0;

	// Format and mount the file system
	if (crud_format() || crud_mount()) {
		logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : Failure on format or mount operation.");
		return(-1);
	}

	// Start by opening a file
	fh = crud_open("temp_file.txt");
	if (fh == -1) {
		logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : Failure open operation.");
		return(-1);
	}

	// Now do a bunch of operations
	for (i=0; i<CRUD_IO_UNIT_TEST_ITERATIONS; i++) {

		// Pick a random command
		if (cio_utest_length == 0) {
			cmd = CIO_UNIT_TEST_WRITE;
		} else {
			cmd = getRandomValue(CIO_UNIT_TEST_READ, CIO_UNIT_TEST_SEEK);
		}

		// Execute the command
		switch (cmd) {

		case CIO_UNIT_TEST_READ: // read a random set of data
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "CRUD_IO_UNIT_TEST : read %d at position %d", bytes, cio_utest_position);
			bytes = crud_read(fh, tbuf, count);
			if (bytes == -1) {
				logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : Read failure.");
				return(-1);
			}

			// Compare to what we expected
			if (cio_utest_position+count > cio_utest_length) {
				expected = cio_utest_length-cio_utest_position;
			} else {
				expected = count;
			}
			if (bytes != expected) {
				logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : short/long read of [%d!=%d]", bytes, expected);
				return(-1);
			}
			if ( (bytes > 0) && (memcmp(&cio_utest_buffer[cio_utest_position], tbuf, bytes)) ) {

				bufToString((unsigned char *)tbuf, bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST R: %s", lstr);
				bufToString((unsigned char *)&cio_utest_buffer[cio_utest_position], bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST U: %s", lstr);

				logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : read data mismatch (%d)", bytes);
				return(-1);
			}
			logMessage(LOG_INFO_LEVEL, "CRUD_IO_UNIT_TEST : read %d match", bytes);


			// update the position pointer
			cio_utest_position += bytes;
			break;

		case CIO_UNIT_TEST_APPEND: // Append data onto the end of the file
			// Create random block, check to make sure that the write is not too large
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			if (cio_utest_length+count >= CRUD_MAX_OBJECT_SIZE) {

				// Log, seek to end of file, create random value
				logMessage(LOG_INFO_LEVEL, "CRUD_IO_UNIT_TEST : append of %d bytes [%x]", count, ch);
				logMessage(LOG_INFO_LEVEL, "CRUD_IO_UNIT_TEST : seek to position %d", cio_utest_length);
				if (crud_seek(fh, cio_utest_length)) {
					logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : seek failed [%d].", cio_utest_length);
					return(-1);
				}
				cio_utest_position = cio_utest_length;
				memset(&cio_utest_buffer[cio_utest_position], ch, count);

				// Now write
				bytes = crud_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes != count) {
					logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : append failed [%d].", count);
					return(-1);
				}
				cio_utest_length = cio_utest_position += bytes;
			}
			break;

		case CIO_UNIT_TEST_WRITE: // Write random block to the file
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			// Check to make sure that the write is not too large
			if (cio_utest_length+count < CRUD_MAX_OBJECT_SIZE) {
				// Log the write, perform it
				logMessage(LOG_INFO_LEVEL, "CRUD_IO_UNIT_TEST : write of %d bytes [%x]", count, ch);
				memset(&cio_utest_buffer[cio_utest_position], ch, count);
				bytes = crud_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes!=count) {
					logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : write failed [%d].", count);
					return(-1);
				}
				cio_utest_position += bytes;
				if (cio_utest_position > cio_utest_length) {
					cio_utest_length = cio_utest_position;
				}
			}
			break;

		case CIO_UNIT_TEST_SEEK:
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "CRUD_IO_UNIT_TEST : seek to position %d", count);
			if (crud_seek(fh, count)) {
				logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : seek failed [%d].", count);
				return(-1);
			}
			cio_utest_position = count;
			break;

		default: // This should never happen
			CMPSC_ASSERT0(0, "CRUD_IO_UNIT_TEST : illegal test command.");
			break;

		}

#if DEEP_DEBUG
		// VALIDATION STEP: ENSURE OUR LOCAL IS LIKE OBJECT STORE
		CrudRequest request;
		CrudResponse response;
		CrudOID oid;
		CRUD_REQUEST_TYPES req;
		uint32_t length;
		uint8_t res, flags;

		// Make a fake request to get file handle, then check it
		request = construct_crud_request(crud_file_table[0].object_id, CRUD_READ, CRUD_MAX_OBJECT_SIZE, CRUD_NULL_FLAG, 0);
		response = crud_bus_request(request, tbuf);
		if ((deconstruct_crud_request(response, &oid, &req, &length, &flags, &res) != 0) || (res != 0))  {
			logMessage(LOG_ERROR_LEVEL, "Read failure, bad CRUD response [%x]", response);
			return(-1);
		}
		if ( (cio_utest_length != length) || (memcmp(cio_utest_buffer, tbuf, length)) ) {
			logMessage(LOG_ERROR_LEVEL, "Buffer/Object cross validation failed [%x]", response);
			bufToString((unsigned char *)tbuf, length, (unsigned char *)lstr, 1024 );
			logMessage(LOG_INFO_LEVEL, "CIO_UTEST VR: %s", lstr);
			bufToString((unsigned char *)cio_utest_buffer, length, (unsigned char *)lstr, 1024 );
			logMessage(LOG_INFO_LEVEL, "CIO_UTEST VU: %s", lstr);
			return(-1);
		}

		// Print out the buffer
		bufToString((unsigned char *)cio_utest_buffer, cio_utest_length, (unsigned char *)lstr, 1024 );
		logMessage(LOG_INFO_LEVEL, "CIO_UTEST: %s", lstr);
#endif

	}

	// Close the files and cleanup buffers, assert on failure
	if (crud_close(fh)) {
		logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : Failure read comparison block.", fh);
		return(-1);
	}
	free(cio_utest_buffer);
	free(tbuf);

	// Format and mount the file system
	if (crud_unmount()) {
		logMessage(LOG_ERROR_LEVEL, "CRUD_IO_UNIT_TEST : Failure on unmount operation.");
		return(-1);
	}

	// Return successfully
	return(0);
}

































