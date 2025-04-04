#ifndef CARDWRITER_H
#define CARDWRITER_H

#include "VCParser.h"

/** Function to write a Card object to a file
 *@pre Card object exists, is not NULL, and is valid
 *@post Card has not been modified in any way, and a file has been created
 *@return OK if successful, WRITE_ERROR if there was an error writing the file
 *@param fileName - a string containing the name of the file to write to
 *@param obj - a pointer to a Card struct that is to be written to the file
 **/
VCardErrorCode writeCard(const char* fileName, const Card* obj);

/** Function to validate a Card object
 *@pre Card object exists and is not NULL
 *@post Card has not been modified in any way
 *@return OK if the card is valid, or an error code if invalid
 *@param obj - a pointer to a Card struct that is to be validated
 **/
VCardErrorCode validateCard(const Card* obj);

#endif 