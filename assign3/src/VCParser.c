#include "VCParser.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h> 


static char* dupString(const char* str) {
    if (str == NULL) return NULL;
    char* dup = malloc(strlen(str) + 1);
    if (dup != NULL) {
        strcpy(dup, str);
    }
    return dup;
}


static bool isValidExtension(const char* fileName);
static bool isCompoundProperty(const char* name);
static char* trimWhitespace(char* str);
static bool addValuesToList(List* list, const char* value, char delimiter);
static Parameter* createParameter(const char* name, const char* value);
static char* parseParameters(const char* propNameStr, Property* prop);
static DateTime* createDateTime(const char* value, bool isText);
static Property* createProperty(const char* name, const char* value);
static char* readFoldedLine(FILE* file, char* buffer, size_t bufSize);
static char* listToString(List* list);
static bool appendToBuffer(char** buffer, size_t* bufSize, size_t* currentLen, const char* str);


static bool isValidExtension(const char* fileName) {
    const char* dot = strrchr(fileName, '.');
    if (dot == NULL) return false;
    return (strcasecmp(dot, ".vcf") == 0 || strcasecmp(dot, ".vcard") == 0);
}

static bool isCompoundProperty(const char* name) {
    if (name == NULL) return false;
    return (strcasecmp(name, "N") == 0 || strcasecmp(name, "ADR") == 0);  
}

static char* trimWhitespace(char* str) {
    if (str == NULL) return NULL;
    
    // trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;  // All spaces
    
    // trim trailing space
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

//split a string by delimiter and add each part to a List
static bool addValuesToList(List* list, const char* value, char delimiter) {
    if (list == NULL || value == NULL) return false;

    //for compound values (not phone numbers with extensions)
    if (delimiter == ';') {
        char* valueCopy = dupString(value);
        if (valueCopy == NULL) return false;

        bool success = true;
        const char* start = value;
        const char* end;

        //process each field
        while (*start != '\0' && success) {
            //find the next delimiter or end of string
            end = strchr(start, delimiter);
            if (end == NULL) {
                end = start + strlen(start);
            }

            size_t fieldLen = end - start;
            char* field = malloc(fieldLen + 1);
            if (field == NULL) {
                success = false;
                break;
            }
            strncpy(field, start, fieldLen);
            field[fieldLen] = '\0';

            char* trimmed = trimWhitespace(field);
            char* val = dupString(trimmed);
            free(field);

            if (val == NULL) {
                success = false;
                break;
            }
            insertBack(list, val);

            // move to next field
            start = *end ? end + 1 : end;
        }

        free(valueCopy);
        return success;
    } else {
        // for non-compound values just add the whole value
        char* val = dupString(value);
        if (val == NULL) return false;
        insertBack(list, val);
        return true;
    }
}

static Parameter* createParameter(const char* name, const char* value) {
    if (name == NULL || value == NULL) return NULL;

    Parameter* param = malloc(sizeof(Parameter));
    if (param == NULL) return NULL;

    //allocate and copy name
    param->name = dupString(name);
    if (param->name == NULL) {
        free(param);
        return NULL;
    }

    //allocate and copy value
    param->value = dupString(value);
    if (param->value == NULL) {
        free(param->name);
        free(param);
        return NULL;
    }

    return param;
}

//parse parameters from a property name string
static char* parseParameters(const char* propNameStr, Property* prop) {
    if (propNameStr == NULL || prop == NULL) return NULL;

    char* nameCopy = dupString(propNameStr);
    if (nameCopy == NULL) return NULL;

    char* baseName = strtok(nameCopy, ";");
    if (baseName == NULL) {
        free(nameCopy);
        return NULL;
    }

    //store the base name to return
    char* result = dupString(baseName);
    if (result == NULL) {
        free(nameCopy);
        return NULL;
    }

    //parse parameters
    char* paramStr = strtok(NULL, ";");
    while (paramStr != NULL) {
        // Split parameter at '='
        char* equals = strchr(paramStr, '=');
        if (equals == NULL) {
            //parameter has no value - invalid
            free(result);
            free(nameCopy);
            return NULL;
        }

        *equals = '\0';
        char* paramName = trimWhitespace(paramStr);
        char* paramValue = trimWhitespace(equals + 1);

        //check for empty parameter name or value
        if (strlen(paramName) == 0 || strlen(paramValue) == 0) {
            free(result);
            free(nameCopy);
            return NULL;
        }

        // create and add parameter
        Parameter* param = createParameter(paramName, paramValue);
        if (param == NULL) {
            free(result);
            free(nameCopy);
            return NULL;
        }
        insertBack(prop->parameters, param);
        paramStr = strtok(NULL, ";");
    }

    free(nameCopy);
    return result;
}

static char* createEmptyString() {
    char* str = malloc(1);
    if (str != NULL) {
        str[0] = '\0';
    }
    return str;
}

static DateTime* initializeDateTime() {
    DateTime* dt = malloc(sizeof(DateTime));
    if (dt == NULL) return NULL;

    dt->UTC = false;
    dt->isText = false;
    dt->date = NULL;
    dt->time = NULL;
    dt->text = NULL;
    return dt;
}

static DateTime* createDateTime(const char* value, bool isText) {
    if (value == NULL) return NULL;

    DateTime* dt = initializeDateTime();
    if (dt == NULL) return NULL;
    dt->isText = isText;

    // handling text values
    if (isText) {
        dt->text = dupString(value);
        dt->date = createEmptyString();
        dt->time = createEmptyString();
        if (!dt->text || !dt->date || !dt->time) {
            deleteDate(dt);
            return NULL;
        }
        return dt;
    }

    // non-text values
    dt->text = createEmptyString();
    if (!dt->text) {
        deleteDate(dt);
        return NULL;
    }

    //time-only value (starts with T)
    if (value[0] == 'T') {
        dt->date = createEmptyString();
        dt->time = dupString(value + 1);  //skip the 'T'
        if (!dt->date || !dt->time) {
            deleteDate(dt);
            return NULL;
        }
        return dt;
    }

    const char* tPos = strchr(value, 'T');
    
    //allocate and copy the date portion
    if (tPos) {
        size_t dateLen = tPos - value;
        dt->date = malloc(dateLen + 1);
        if (!dt->date) {
            deleteDate(dt);
            return NULL;
        }
        strncpy(dt->date, value, dateLen);
        dt->date[dateLen] = '\0';
        
        //copy the time portion after T
        dt->time = dupString(tPos + 1);
    } else {
        //date only
        dt->date = dupString(value);
        dt->time = createEmptyString();
    }

    if (!dt->date || !dt->time) {
        deleteDate(dt);
        return NULL;
    }

    return dt;
}

static bool validateParameters(const char* name) {
    if (name == NULL) return false;

    const char* semicolon = strchr(name, ';');
    while (semicolon != NULL) {
        const char* equals = strchr(semicolon + 1, '=');
        const char* nextSemicolon = strchr(semicolon + 1, ';');
        
        // if we found an equals sign before the next semicolon (or end of string)
        if (equals != NULL && (nextSemicolon == NULL || equals < nextSemicolon)) {
            // check if there's a value after the equals sign
            const char* valueStart = equals + 1;
            while (*valueStart && isspace(*valueStart)) valueStart++;
            if (*valueStart == '\0' || *valueStart == ';') {
                return false;  //empty value after equals sign
            }
        } else if (equals == NULL || (nextSemicolon != NULL && equals > nextSemicolon)) {
            return false;  //parameter without equals sign
        }
        semicolon = nextSemicolon;
    }
    return true;
}

static Property* createProperty(const char* name, const char* value) {
    if (name == NULL || value == NULL) return NULL;

    if (!validateParameters(name)) return NULL;

    Property* prop = malloc(sizeof(Property));
    if (prop == NULL) return NULL;

    // initialize to NULL 
    prop->name = NULL;
    prop->group = NULL;
    prop->parameters = NULL;
    prop->values = NULL;

    prop->parameters = initializeList(parameterToString, deleteParameter, compareParameters);
    prop->values = initializeList(valueToString, deleteValue, compareValues);
    if (prop->parameters == NULL || prop->values == NULL) {
        deleteProperty(prop);
        return NULL;
    }

    // make a copy of the name to parse group and parameters
    char* nameCopy = dupString(name);
    if (nameCopy == NULL) {
        deleteProperty(prop);
        return NULL;
    }

    // check for group
    char* dot = strchr(nameCopy, '.');
    if (dot != NULL) {
        *dot = '\0';  //split at the dot
        prop->group = dupString(nameCopy);
        if (prop->group == NULL) {
            free(nameCopy);
            deleteProperty(prop);
            return NULL;
        }
        //move to the part after the dot for parameter parsing
        char* baseName = parseParameters(dot + 1, prop);
        if (baseName == NULL) {
            free(nameCopy);
            deleteProperty(prop);
            return NULL;
        }
        prop->name = baseName;
    } else {
        //no group
        prop->group = dupString("");
        if (prop->group == NULL) {
            free(nameCopy);
            deleteProperty(prop);
            return NULL;
        }

        char* baseName = parseParameters(nameCopy, prop);
        if (baseName == NULL) {
            free(nameCopy);
            deleteProperty(prop);
            return NULL;
        }
        prop->name = baseName;
    }

    free(nameCopy);

    // add values
    if (isCompoundProperty(prop->name)) {
        if (!addValuesToList(prop->values, value, ';')) {
            deleteProperty(prop);
            return NULL;
        }
    } else {
        char* val = dupString(value);
        if (val == NULL) {
            deleteProperty(prop);
            return NULL;
        }
        insertBack(prop->values, val);
    }

    return prop;
}

// check if a line has proper CRLF ending
static bool hasValidLineEnding(const char* line, size_t len) {
    return (len >= 2 && line[len-2] == '\r' && line[len-1] == '\n');
}

// check if a line is a continuation line
static bool isContinuationLine(const char* line) {
    return (line[0] == ' ' || line[0] == '\t');
}

static char* readFoldedLine(FILE* file, char* buffer, size_t bufSize) {
    if (file == NULL || buffer == NULL || bufSize == 0) {
        return NULL;
    }

    if (fgets(buffer, bufSize, file) == NULL) {
        return NULL;
    }

    size_t len = strlen(buffer);
    if (!hasValidLineEnding(buffer, len)) {
        buffer[0] = '\0';  // clear buffer to indicate error
        return NULL;
    }

    // remove CRLF
    buffer[len-2] = '\0';

    // look ahead for folded lines
    char nextLine[1000];
    long pos = ftell(file);  // remember current position

    while (fgets(nextLine, sizeof(nextLine), file)) {
        // check if this line starts with a space or tab
        if (!isContinuationLine(nextLine)) {
            fseek(file, pos, SEEK_SET);
            break;
        }

        // check for proper CRLF in continuation line
        len = strlen(nextLine);
        if (!hasValidLineEnding(nextLine, len)) {
            buffer[0] = '\0';  
            return NULL;
        }

        nextLine[len-2] = '\0';

        // skip the leading whitespace in nextLine
        char* foldedContent = nextLine + 1;
        while (*foldedContent && isspace(*foldedContent)) {
            foldedContent++;
        }

        // calculate available space in buffer
        size_t currentLen = strlen(buffer);
        size_t remainingSpace = bufSize - currentLen - 1; 

        // check if space is sufficient
        if (strlen(foldedContent) < remainingSpace) {
            strcat(buffer, foldedContent);
            pos = ftell(file);  // update position after successful fold
        } else {
            fseek(file, pos, SEEK_SET);
            break;
        }

        pos = ftell(file);  // remember position for next iteration
    }

    return buffer;
}

// (VERSION, FN, BDAY, ANNIVERSARY)
static VCardErrorCode handleSpecialProperty(Card* card, const char* propName, const char* propValue, bool* foundFN, bool* foundVersion, bool* validVersion) {
    // extract base property name (before any parameters)
    char* basePropName = dupString(propName);
    if (basePropName == NULL) {
        return OTHER_ERROR;
    }
    
    // find first semicolon and truncate there to get base name
    char* semicolon = strchr(basePropName, ';');
    if (semicolon != NULL) {
        *semicolon = '\0';
    }

    VCardErrorCode result = OK;

    if (strcasecmp(basePropName, "VERSION") == 0) {
        if (*foundVersion) {  // duplicate VERSION
            result = INV_CARD;
        } else {
            *foundVersion = true;
            if (strcmp(propValue, "4.0") == 0) {
                *validVersion = true;
            }
        }
    }
    else if (strcasecmp(basePropName, "FN") == 0) {
        *foundFN = true;
        if (card->fn == NULL) {  // store first FN encountered
            card->fn = createProperty(propName, propValue);
            if (card->fn == NULL) {
                result = OTHER_ERROR;
            }
        }
    }
    // Handle ANNIVERSARY property
    else if (strcasecmp(basePropName, "ANNIVERSARY") == 0) {
        if (card->anniversary != NULL) {  // already have an anniversary
            result = INV_CARD;
        } else {
            // temporary property to parse parameters
            Property* tempProp = createProperty(propName, propValue);
            if (tempProp == NULL) {
                result = OTHER_ERROR;
            } else {
                //check for VALUE=text parameter
                bool isTextValue = false;
                ListIterator iter = createIterator(tempProp->parameters);
                void* elem;
                while ((elem = nextElement(&iter)) != NULL) {
                    Parameter* param = (Parameter*)elem;
                    if (strcasecmp(param->name, "VALUE") == 0 && strcasecmp(param->value, "text") == 0) {
                        isTextValue = true;
                        break;
                    }
                }

                card->anniversary = createDateTime(propValue, isTextValue);
                if (card->anniversary == NULL) {
                    result = OTHER_ERROR;
                }

                deleteProperty(tempProp);
            }
        }
    } else if (strcasecmp(basePropName, "BDAY") == 0) {
        if (card->birthday != NULL) {  // already have a birthday
            result = INV_CARD;
        } else {
            // temporary property to parse parameters
            Property* tempProp = createProperty(propName, propValue);
            if (tempProp == NULL) {
                result = OTHER_ERROR;
            } else {
                bool isTextValue = false;
                ListIterator iter = createIterator(tempProp->parameters);
                void* elem;
                while ((elem = nextElement(&iter)) != NULL) {
                    Parameter* param = (Parameter*)elem;
                    if (strcasecmp(param->name, "VALUE") == 0 && strcasecmp(param->value, "text") == 0) {
                        isTextValue = true;
                        break;
                    }
                }

                card->birthday = createDateTime(propValue, isTextValue);
                if (card->birthday == NULL) {
                    result = OTHER_ERROR;
                }

                deleteProperty(tempProp);
            }
        }
    }

    free(basePropName);
    return result;
}

static VCardErrorCode validatePropertyLine(const char* line, char** propName, char** propValue) {
    // make a copy of the line since we'll modify it
    char* lineCopy = dupString(line);
    if (lineCopy == NULL) {
        return OTHER_ERROR;
    }

    // find the colon separator
    char* colon = strchr(lineCopy, ':');
    if (colon == NULL) {
        free(lineCopy);
        return INV_PROP;
    }

    // split -> property name and value
    *colon = '\0';
    char* tempName = trimWhitespace(lineCopy);
    char* tempValue = trimWhitespace(colon + 1);

    // check for empty strings before making copies
    if (strlen(tempName) == 0 || strlen(tempValue) == 0) {
        free(lineCopy);
        return INV_PROP;
    }

    // make copies of the strings since we're returning them
    *propName = dupString(tempName);
    *propValue = dupString(tempValue);
    
    // check if either allocation failed
    if (*propName == NULL || *propValue == NULL) {
        free(lineCopy);
        if (*propName) free(*propName);
        if (*propValue) free(*propValue);
        *propName = NULL;
        *propValue = NULL;
        return OTHER_ERROR;
    }

    free(lineCopy);
    return OK;
}

VCardErrorCode createCard(char* fileName, Card** obj) {
    if (fileName == NULL || obj == NULL || fileName[0] == '\0') {
        return INV_FILE;
    }

    if (!isValidExtension(fileName)) {
        return INV_FILE;
    }

    FILE* file = fopen(fileName, "r");
    if (file == NULL) {
        return INV_FILE;
    }

    // init the card
    Card* newCard = malloc(sizeof(Card));
    if (newCard == NULL) {
        fclose(file);
        return OTHER_ERROR;
    }

    newCard->fn = NULL;
    newCard->optionalProperties = initializeList(propertyToString, deleteProperty, compareProperties);
    newCard->birthday = NULL;
    newCard->anniversary = NULL;

    if (newCard->optionalProperties == NULL) {
        free(newCard);
        fclose(file);
        return OTHER_ERROR;
    }

    //required properties
    bool foundBegin = false;
    bool foundEnd = false;
    bool foundFN = false;
    bool foundVersion = false;
    bool validVersion = false;

    char line[1000];  // buffer for reading lines
    
    // read until BEGIN:VCARD
    while (readFoldedLine(file, line, sizeof(line))) {
        char* trimmed = trimWhitespace(line);
        if (strcasecmp(trimmed, "BEGIN:VCARD") == 0) {
            foundBegin = true;
            break;
        }
    }

    // check if we failed to find BEGIN:VCARD due to invalid line endings
    if (!foundBegin && line[0] == '\0') {
        deleteCard(newCard);
        fclose(file);
        *obj = NULL;
        return INV_CARD;
    }

    if (!foundBegin) {
        deleteCard(newCard);
        fclose(file);
        *obj = NULL;
        return INV_CARD;
    }

    // PROCESS CARD CONTENT

    while (readFoldedLine(file, line, sizeof(line))) {
        // check for invalid line endings
        if (line[0] == '\0') {
            deleteCard(newCard);
            fclose(file);
            *obj = NULL;
            return INV_CARD;
        }

        char* trimmed = trimWhitespace(line);

        if (strcasecmp(trimmed, "END:VCARD") == 0) {
            foundEnd = true;
            break;
        }

        char* propName;
        char* propValue;
        VCardErrorCode err = validatePropertyLine(trimmed, &propName, &propValue);
        if (err != OK) {
            deleteCard(newCard);
            fclose(file);
            *obj = NULL;
            return err;
        }

        // temporary property to validate parameters
        Property* tempProp = createProperty(propName, propValue);
        if (tempProp == NULL) {
            //check if it was due to invalid parameters
            if (strchr(propName, ';') != NULL) {
                free(propName);
                free(propValue);
                deleteCard(newCard);
                fclose(file);
                *obj = NULL;
                return INV_PROP;
            }
            // else its a memory allocation error
            free(propName);
            free(propValue);
            deleteCard(newCard);
            fclose(file);
            *obj = NULL;
            return OTHER_ERROR;
        }

        // check for parameter validation error
        if (tempProp->name == NULL || tempProp->name[0] == '\0') {
            free(propName);
            free(propValue);
            deleteProperty(tempProp);
            deleteCard(newCard);
            fclose(file);
            *obj = NULL;
            return INV_PROP;
        }

        // handle special properties
        err = handleSpecialProperty(newCard, propName, propValue, &foundFN, &foundVersion, &validVersion);
        if (err != OK) {
            free(propName);
            free(propValue);
            deleteProperty(tempProp);
            deleteCard(newCard);
            fclose(file);
            *obj = NULL;
            return err;
        }

        // Extract base property name (before any parameters)
        char* basePropName = dupString(propName);
        if (basePropName == NULL) {
            free(propName);
            free(propValue);
            deleteProperty(tempProp);
            deleteCard(newCard);
            fclose(file);
            *obj = NULL;
            return OTHER_ERROR;
        }
        
        // Find first semicolon and truncate there to get base name
        char* semicolon = strchr(basePropName, ';');
        if (semicolon != NULL) {
            *semicolon = '\0';
        }

        // if not a special property, add to optional properties
        if (strcasecmp(basePropName, "VERSION") != 0 && strcasecmp(basePropName, "FN") != 0 && 
            strcasecmp(basePropName, "BDAY") != 0 && strcasecmp(basePropName, "ANNIVERSARY") != 0) {
            Property* prop = createProperty(propName, propValue);
            if (prop == NULL) {
                free(basePropName);
                free(propName);
                free(propValue);
                deleteProperty(tempProp);
                deleteCard(newCard);
                fclose(file);
                *obj = NULL;
                return OTHER_ERROR;
            }
            insertBack(newCard->optionalProperties, prop);
        }

        free(basePropName);
        free(propName);
        free(propValue);
        deleteProperty(tempProp);
    }

    fclose(file);

    // required properties
    if (!foundEnd || !foundFN || !foundVersion || !validVersion) {
        deleteCard(newCard);
        *obj = NULL;
        return INV_CARD;
    }

    *obj = newCard;
    return OK;
}

void deleteCard(Card* obj) {
    if (obj == NULL) {
        return;
    }

    if (obj->fn != NULL) {
        deleteProperty(obj->fn);
    }

    if (obj->optionalProperties != NULL) {
        freeList(obj->optionalProperties);
    }

    if (obj->birthday != NULL) {
        deleteDate(obj->birthday);
    }

    if (obj->anniversary != NULL) {
        deleteDate(obj->anniversary);
    }

    free(obj);
}

// helper to safely append a string to a buffer, reallocating if needed
static bool appendToBuffer(char** buffer, size_t* bufSize, size_t* currentLen, const char* str) {
    if (str == NULL) {
        str = "NULL";
    }
    
    size_t strLen = strlen(str);
    size_t neededSpace = *currentLen + strLen + 1;  // +1 for null terminator
    
    // reallocate if needed
    while (neededSpace > *bufSize) {
        *bufSize *= 2;
        char* newBuf = realloc(*buffer, *bufSize);
        if (newBuf == NULL) {
            return false;
        }
        *buffer = newBuf;
    }
    
    strcat(*buffer, str);
    *currentLen += strLen;
    return true;
}

char* cardToString(const Card* obj) {
    if (obj == NULL) {
        return NULL;
    }

    size_t bufSize = 256;
    size_t currentLen = 0;
    char* result = malloc(bufSize);
    if (result == NULL) {
        return NULL;
    }
    result[0] = '\0';

    // add each component
    if (!appendToBuffer(&result, &bufSize, &currentLen, "Card:\n FN: ")) {
        free(result);
        return NULL;
    }

    // add FN
    char* fnStr = obj->fn ? propertyToString(obj->fn) : NULL;
    if (!appendToBuffer(&result, &bufSize, &currentLen, fnStr)) {
        if (fnStr) free(fnStr);
        free(result);
        return NULL;
    }
    if (fnStr) free(fnStr);

    if (!appendToBuffer(&result, &bufSize, &currentLen, "\n Optional Properties: ")) {
        free(result);
        return NULL;
    }
    char* optionalStr = obj->optionalProperties ? listToString(obj->optionalProperties) : NULL;
    if (!appendToBuffer(&result, &bufSize, &currentLen, optionalStr)) {
        if (optionalStr) free(optionalStr);
        free(result);
        return NULL;
    }
    if (optionalStr) free(optionalStr);

    if (!appendToBuffer(&result, &bufSize, &currentLen, "\n Birthday: ")) {
        free(result);
        return NULL;
    }
    char* birthdayStr = obj->birthday ? dateToString(obj->birthday) : NULL;
    if (!appendToBuffer(&result, &bufSize, &currentLen, birthdayStr)) {
        if (birthdayStr) free(birthdayStr);
        free(result);
        return NULL;
    }
    if (birthdayStr) free(birthdayStr);

    if (!appendToBuffer(&result, &bufSize, &currentLen, "\n Anniversary: ")) {
        free(result);
        return NULL;
    }
    char* anniversaryStr = obj->anniversary ? dateToString(obj->anniversary) : NULL;
    if (!appendToBuffer(&result, &bufSize, &currentLen, anniversaryStr)) {
        if (anniversaryStr) free(anniversaryStr);
        free(result);
        return NULL;
    }
    if (anniversaryStr) free(anniversaryStr);

    //final newline
    if (!appendToBuffer(&result, &bufSize, &currentLen, "\n")) {
        free(result);
        return NULL;
    }

    return result;
}

char* errorToString(VCardErrorCode err) {
    switch(err) {
        case OK:
            return "OK";
        case INV_FILE:
            return "Invalid file";
        case INV_CARD:
            return "Invalid card";
        case INV_PROP:
            return "Invalid property";
        case INV_DT:
            return "Invalid date";
        case WRITE_ERROR:
            return "Write error";
        case OTHER_ERROR:
            return "Other error";
        default:
            return "Unknown error";
    }
}

// ************* List helper functions implementation ***************

void deleteProperty(void* toBeDeleted) {
    if (toBeDeleted == NULL) {
        return;
    }

    Property* prop = (Property*)toBeDeleted;

    if (prop->name) {
        free(prop->name);
    }

    if (prop->group) {
        free(prop->group);
    }

    if (prop->parameters) {
        freeList(prop->parameters);
    }

    if (prop->values) {
        freeList(prop->values);
    }

    free(prop);
}

int compareProperties(const void* first, const void* second) {
    if (first == NULL || second == NULL) {
        return 0;
    }

    Property* p1 = (Property*)first;
    Property* p2 = (Property*)second;

    if (p1->name == NULL || p2->name == NULL) {
        return 0;
    }

    return strcmp(p1->name, p2->name);
}

char* listToString(List* list) {
    if (list == NULL || list->length == 0) {
        char* emptyStr = malloc(1);
        if (emptyStr != NULL) {
            emptyStr[0] = '\0';
        }
        return emptyStr;
    }

    size_t bufSize = 256;
    char* result = malloc(bufSize);
    if (result == NULL) {
        return NULL;
    }
    result[0] = '\0';
    size_t currentLen = 0;

    ListIterator iter = createIterator(list);
    void* elem;
    bool first = true;
    while ((elem = nextElement(&iter)) != NULL) {
        char* str = list->printData(elem);
        if (str != NULL) {
            size_t strLen = strlen(str);
            size_t sepLen = first ? 0 : 1;  // lenggth of separator
            
            // check if we need to increase the buffer size
            while (currentLen + strLen + sepLen + 1 > bufSize) {
                bufSize *= 2;
                char* newBuf = realloc(result, bufSize);
                if (newBuf == NULL) {
                    free(result);
                    free(str);
                    return NULL;
                }
                result = newBuf;
            }

            // add separator if not first element
            if (!first) {
                if (list->printData == parameterToString) {
                    strcat(result, ";");  
                } else {
                    // check if it's a phone number extension
                    if (strstr(str, "ext=") != NULL) {
                        strcat(result, ";");  // use semicolon for extensions
                    } else {
                        strcat(result, ",");  // other values use comma
                    }
                }
                currentLen++;
            }

            strcat(result, str);
            currentLen += strLen;
            free(str);
        }
        first = false;
    }

    return result;
}

char* propertyToString(void* prop) {
    if (prop == NULL) return NULL;
    Property* property = (Property*)prop;
    
    // Calculate initial buffer size
    size_t bufSize = strlen(property->name) + 2;  // +2 for ':' and '\0'
    if (strlen(property->group) > 0) {
        bufSize += strlen(property->group) + 1;  // +1 for '.'
    }
    
    // Add space for parameters
    ListIterator paramIter = createIterator(property->parameters);
    void* elem;
    while ((elem = nextElement(&paramIter)) != NULL) {
        Parameter* param = (Parameter*)elem;
        bufSize += strlen(param->name) + strlen(param->value) + 3;  // +3 for ';', '=', and quotes if needed
    }
    
    // Add space for values
    ListIterator valueIter = createIterator(property->values);
    while ((elem = nextElement(&valueIter)) != NULL) {
        char* value = (char*)elem;
        bufSize += strlen(value) + 1;  // +1 for delimiter
    }
    
    // Allocate buffer with extra space for potential quotes and null terminator
    bufSize = bufSize * 2 + 1;  // Double for quotes, +1 for null terminator
    char* str = malloc(bufSize);
    if (str == NULL) return NULL;
    
    // Start with group if present
    size_t pos = 0;
    if (property->group != NULL && strlen(property->group) > 0) {
        pos += snprintf(str + pos, bufSize - pos, "%s.", property->group);
        if (pos >= bufSize) {
            free(str);
            return NULL;
        }
    }
    
    // Add property name
    pos += snprintf(str + pos, bufSize - pos, "%s", property->name);
    if (pos >= bufSize) {
        free(str);
        return NULL;
    }
    
    // Add parameters
    paramIter = createIterator(property->parameters);
    while ((elem = nextElement(&paramIter)) != NULL) {
        Parameter* param = (Parameter*)elem;
        // Check if value needs quotes (contains comma or semicolon)
        bool needsQuotes = (strchr(param->value, ',') != NULL || strchr(param->value, ';') != NULL);
        if (needsQuotes && !strchr(param->value, '"')) {  // Only add quotes if not already quoted
            pos += snprintf(str + pos, bufSize - pos, ";%s=\"%s\"", param->name, param->value);
        } else {
            pos += snprintf(str + pos, bufSize - pos, ";%s=%s", param->name, param->value);
        }
        if (pos >= bufSize) {
            free(str);
            return NULL;
        }
    }
    
    // Add colon before values
    if (pos + 1 >= bufSize) {
        free(str);
        return NULL;
    }
    str[pos++] = ':';
    
    // Add values with appropriate delimiter
    bool isFirst = true;
    bool isCompound = isCompoundProperty(property->name);
    char delimiter = isCompound ? ';' : ',';
    
    valueIter = createIterator(property->values);
    while ((elem = nextElement(&valueIter)) != NULL) {
        char* value = (char*)elem;
        if (!isFirst) {
            if (pos + 1 >= bufSize) {
                free(str);
                return NULL;
            }
            str[pos++] = delimiter;
        }
        size_t valueLen = strlen(value);
        if (pos + valueLen >= bufSize) {
            free(str);
            return NULL;
        }
        memcpy(str + pos, value, valueLen);
        pos += valueLen;
        isFirst = false;
    }
    
    str[pos] = '\0';
    return str;
}

void deleteParameter(void* toBeDeleted) {
    if (toBeDeleted == NULL) {
        return;
    }

    Parameter* param = (Parameter*)toBeDeleted;
    if (param->name) {
        free(param->name);
    }
    if (param->value) {
        free(param->value);
    }
    free(param);
}

int compareParameters(const void* first, const void* second) {
    if (first == NULL || second == NULL) {
        return 0;
    }

    Parameter* p1 = (Parameter*)first;
    Parameter* p2 = (Parameter*)second;

    if (p1->name == NULL || p2->name == NULL) {
        return 0;
    }

    return strcmp(p1->name, p2->name);
}

char* parameterToString(void* param) {
    if (param == NULL) {
        return NULL;
    }

    Parameter* parameter = (Parameter*)param;
    if (parameter->name == NULL) return NULL;

    size_t totalLen = strlen(parameter->name) + 2;  // +2 for '=' and '\0'
    if (parameter->value) {
        totalLen += strlen(parameter->value);
    }

    char* result = malloc(totalLen);
    if (result == NULL) {
        return NULL;
    }

    if (parameter->value) {
        sprintf(result, "%s=%s", parameter->name, parameter->value);
    } else {
        sprintf(result, "%s=", parameter->name);
    }
    return result;
}

void deleteValue(void* toBeDeleted) {
    if (toBeDeleted == NULL) {
        return;
    }
    free(toBeDeleted); 
}

int compareValues(const void* first, const void* second) {
    if (first == NULL || second == NULL) {
        return 0;
    }

    char* str1 = (char*)first;
    char* str2 = (char*)second;
    return strcmp(str1, str2);
}

char* valueToString(void* val) {
    if (val == NULL) {
        return NULL;
    }

    char* value = (char*)val;
    char* result = malloc(strlen(value) + 1);
    if (result == NULL) {
        return NULL;
    }

    strcpy(result, value);
    return result;
}

void deleteDate(void* toBeDeleted) {
    if (toBeDeleted == NULL) {
        return;
    }

    DateTime* dt = (DateTime*)toBeDeleted;
    if (dt->date) {
        free(dt->date);
    }
    if (dt->time) {
        free(dt->time);
    }
    if (dt->text) {
        free(dt->text);
    }
    free(dt);
}

int compareDates(const void* first, const void* second) {
    // not required yet
    return 0;
}

char* dateToString(void* date) {
    if (date == NULL) {
        return NULL;
    }

    DateTime* dt = (DateTime*)date;
    char* result;

    if (dt->isText) {
        // for text values just return the text directly
        result = malloc(strlen(dt->text) + 1);
        if (result == NULL) {
            return NULL;
        }
        strcpy(result, dt->text);
    } else {
        // for date/time values combine date and time
        size_t totalLen = 1;  // for null terminator
        if (dt->date) totalLen += strlen(dt->date);
        if (dt->time) totalLen += strlen(dt->time);
        if (dt->UTC) totalLen += 4;  // " UTC"

        result = malloc(totalLen);
        if (result == NULL) {
            return NULL;
        }

        result[0] = '\0';  
        if (dt->date) strcat(result, dt->date);
        if (dt->time) strcat(result, dt->time);
        if (dt->UTC) strcat(result, " UTC");
    }

    return result;
} 