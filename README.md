# CRUD_Device_Driver

This is a user-space device driver for a filesystem built on top of a object storage device. At the highest level, I translated file system commands into storage array commands. These file system commands include open, read, write, and close for files that are written to my file system driver. These operations perform the same as the normal UNIX I/O operations,
with the caveat that they direct file contents to the object storage device instead of the host filesystem. 

The arrangement of software is as follows:

   SIMULATED APPLICATION (PROVIDED) 
<--> 
   (filesystem commands, e.g., open, read ..)
<--> 
   FILESYSTEM DRIVER (MY CODE)
<--> 
   (CRUD commands, e.g., CRUD_CREATE, CRUD_READ..)
<--> 
   CRUD INTERFACE (PROVIDED)
   OBJECT STORE (PROVIDED)

# Object Store/CRUD Interface
An object store is a virtual device that stores variable sized blocks of data called objects. Software that uses
the store create and manipulate objects on the device in a way very similar to normal disk drives (see the
CRUD interface commands below), except that they manipulate objects instead of disk blocks. Each object is
referenced by a uniquely identified by an integer value assigned by the object store called an object identifier.

The object store I have built on top of exports a CRUD interface; it supports creating objects, reading
objects, updating objects, and deleting objects. Note that this code for this interface was provided in library
form. Also, objects have immutable size; once allocated, the contents can be changed repeatedly, but the size
can never change. Thus, any operation which would require a change to the object size must be performed by
deleting an old object and creating a new one.

I programmed the CRUD interface to make requests to the object store, as defined in the file crud driver.h,.
This interface contains a single function call that accepts two arguments, a 64-bit CRUD bus request value
(with type CrudReuqest) and a pointer to a variable-sized buffer;
      CrudResponse crud bus request(CrudRequest request, void *buf);

#The CRUD commands are as follows:
• CRUD INIT - This command initializes the object store and readies it for use in file operations. In addition to initializing the object store, this command will now also load any saved state from crud_content.crd (if that file exists).
• CRUD CREATE - This command creates an object whose length is defined in a buffer request value defined
below. The buffer passed to the function contains data of that length. Note that the CRUD interface
copies the passed data into an internal structure, so I am responsible for managing any buffers passed
to it. If successful, the operation will return the new object ID in the response object value.
• CRUD READ - This command reads an object (in its entirety) from the object store and copies its contents
in passed buffer. The length field should be set to the length of the passed buffer (because I don’t know
how big the object is going to be, I always pass in a buffer of size CRUD MAX OBJECT SIZE).
The returned response value indicates the length of the object read.
• CRUD UPDATE - This command will update the contents of an object. Note that the object size CAN
NEVER change. Thus, the call will fail unless the buffer sent is the same size as the original object
created.
• CRUD_FORMAT: This command will delete crud_content.crd and all objects in the object
store (including the priority object).
• CRUD DELETE - This command will save the contents of the object store to the crud_content.crd
file, creating it if it does not exist. 

# CrudRequest Definition
The CrudRequest value passed to the bus function is a 64-bit value that defines a request and its parameters, as
follows (by convention bit zero is the most significant bit):
63:33 = OID (32 bits)
32:29 = Reqest (4 bits)
28:5  = Length (24 bits)
 3:1  = Flags (3 bits)
  0   = Result Code (1 bit)
  
Note that the 64-bit response value (CrudResponse) returned from a call to the CRUD interface has the same
fields, with slightly different meaning as described below. The fields of the value are:
• OID - This is the object identifier of the object you are executing a command on. For object creates, the
returned object identifier is the new object ID.
• Request - This is the request type of the command being executed. The value can be CRUD INIT,
CRUD CREATE, CRUD READ, CRUD UPDATE, or CRUD DELETE.
• Length - This is the length of the object. On updates and creates, the request value will include the size
of the object attempting to create or update. On reads, the length should be the size of the buffer
handing to the device (the maximum size of the object that can be read). For all calls, the returned
size is the size of the object read, written, or updated.
• Flags - These are unused for this assignment.
• R - Result code. Only used in the response, this is success status of the command execution, where 0
(zero) signifies success, and 1 signifies failure. 

# The Filesystem Driver
The bulk of this project is to develop code for the file-oriented input/output driver that uses the object store.
Conceptually, I translated each of the below file I/O function calls into calls to the previously described
CRUD interface. It was up to me to decide how to implement these functions. The functions
maintain the file contents in exactly the same way as a normal filesystem would. The functions that I
implemented are defined in crud file io.h and crud file io.c and perform as follows:

crud_open -  This call opens a file and returns an integer file handle (to be assigned by me).

crud_close -  This call closes the file referenced by the file handle. 

crud_read - This call reads a count number of bytes from the current position in the file and
places them into the buffer buf. The function returns -1 if failure, or the number
of bytes read if successful. If there are not enough bytes fulfill the read request, it
should only read as many as are available and return the number of bytes read.

crud_write - This call writes a count number of bytes at the current position in the file associated
with the file handle fd from the buffer buf. The function returns -1 if failure, or the
number of written read if successful. When number of bytes to written extends
beyond the end of the file, the size of the file is increased.

crud-seek - This call resets the current position of the file associated with the file handle fd to
the position loc.

# Features
• Track multiple open files simultaneously, by using a file table
• File data that persists between runs of a program, by implementing the basic filesystem operations of
“format,” “mount,” and “unmount”

# File Allocation Table
Each entry in this table represents a single file in the object store and its current status if open. The structure of
a file table entry is defined as CrudFileAllocationType in crud_file_io.h:
    typedef struct {
        char filename[CRUD_MAX_PATH_LENGTH];
        CrudOID object_id;
        uint32_t position;
        uint32_t length;
        uint8_t open;
} CrudFileAllocationType;

and the fields are defined as follows:
• filename: Name of the file, as passed to crud_open; this name, including the terminator, will never be longer than CRUD_MAX_PATH_LENGTH
• object_id: OID of the object which corresponds to this file
• position: Current file position (only for open files)
• length: Length of the file in bytes
• open: Flag that indicates whether the file is currently open (nonzero value) or closed (zero)
