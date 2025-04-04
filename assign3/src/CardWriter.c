#include "CardWriter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

VCardErrorCode writeCard(const char* fileName, const Card* obj) {
    // Input validation
    if (fileName == NULL || obj == NULL) {
        return WRITE_ERROR;
    }

    // Try to open file for writing
    FILE* file = fopen(fileName, "w");
    if (file == NULL) {
        return WRITE_ERROR;
    }

    // Write BEGIN:VCARD
    if (fprintf(file, "BEGIN:VCARD\r\n") < 0) {
        fclose(file);
        return WRITE_ERROR;
    }

    // Write VERSION:4.0
    if (fprintf(file, "VERSION:4.0\r\n") < 0) {
        fclose(file);
        return WRITE_ERROR;
    }

    // Write FN property
    if (obj->fn != NULL) {
        char* fnStr = propertyToString(obj->fn);
        if (fnStr == NULL) {
            fclose(file);
            return WRITE_ERROR;
        }
        if (fprintf(file, "%s\r\n", fnStr) < 0) {
            free(fnStr);
            fclose(file);
            return WRITE_ERROR;
        }
        free(fnStr);
    }

    // Write optional properties
    if (obj->optionalProperties != NULL) {
        ListIterator iter = createIterator(obj->optionalProperties);
        void* elem;
        while ((elem = nextElement(&iter)) != NULL) {
            Property* prop = (Property*)elem;
            char* propStr = propertyToString(prop);
            if (propStr == NULL) {
                fclose(file);
                return WRITE_ERROR;
            }
            if (fprintf(file, "%s\r\n", propStr) < 0) {
                free(propStr);
                fclose(file);
                return WRITE_ERROR;
            }
            free(propStr);
        }
    }

    // Write BDAY if exists
    if (obj->birthday != NULL) {
        if (obj->birthday->isText) {
            if (fprintf(file, "BDAY;VALUE=text:%s\r\n", obj->birthday->text) < 0) {
                fclose(file);
                return WRITE_ERROR;
            }
        } else {
            // Handle time-only case
            if (strlen(obj->birthday->date) == 0 && strlen(obj->birthday->time) > 0) {
                if (fprintf(file, "BDAY:T%s\r\n", obj->birthday->time) < 0) {
                    fclose(file);
                    return WRITE_ERROR;
                }
            } else {
                char* bdayStr = dateToString(obj->birthday);
                if (bdayStr == NULL) {
                    fclose(file);
                    return WRITE_ERROR;
                }
                if (fprintf(file, "BDAY:%s\r\n", bdayStr) < 0) {
                    free(bdayStr);
                    fclose(file);
                    return WRITE_ERROR;
                }
                free(bdayStr);
            }
        }
    }

    // Write ANNIVERSARY if exists
    if (obj->anniversary != NULL) {
        if (obj->anniversary->isText) {
            if (fprintf(file, "ANNIVERSARY;VALUE=text:%s\r\n", obj->anniversary->text) < 0) {
                fclose(file);
                return WRITE_ERROR;
            }
        } else {
            // Handle time-only case
            if (strlen(obj->anniversary->date) == 0 && strlen(obj->anniversary->time) > 0) {
                if (fprintf(file, "ANNIVERSARY:T%s\r\n", obj->anniversary->time) < 0) {
                    fclose(file);
                    return WRITE_ERROR;
                }
            } else {
                char* annivStr = dateToString(obj->anniversary);
                if (annivStr == NULL) {
                    fclose(file);
                    return WRITE_ERROR;
                }
                if (fprintf(file, "ANNIVERSARY:%s\r\n", annivStr) < 0) {
                    free(annivStr);
                    fclose(file);
                    return WRITE_ERROR;
                }
                free(annivStr);
            }
        }
    }

    // Write END:VCARD
    if (fprintf(file, "END:VCARD\r\n") < 0) {
        fclose(file);
        return WRITE_ERROR;
    }

    fclose(file);
    return OK;
}

// Helper function to check if a property name is valid according to vCard spec sections 6.1-6.9.3
static bool isValidPropertyName(const char* name) {
    const char* validNames[] = {
        "FN", "N", "NICKNAME", "PHOTO", "BDAY", "ANNIVERSARY", "GENDER", 
        "ADR", "TEL", "EMAIL", "IMPP", "LANG", "TZ", "GEO", "TITLE", 
        "ROLE", "LOGO", "ORG", "MEMBER", "RELATED", "CATEGORIES", 
        "NOTE", "PRODID", "REV", "SOUND", "UID", "CLIENTPIDMAP", "URL"
    };
    const int numValidNames = sizeof(validNames) / sizeof(validNames[0]);
    
    for (int i = 0; i < numValidNames; i++) {
        if (strcmp(name, validNames[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Helper function to validate a DateTime object
static VCardErrorCode validateDateTime(const DateTime* dt) {
    if (dt == NULL) {
        return OK; // DateTime is optional
    }

    // Check for NULL pointers
    if (dt->date == NULL || dt->time == NULL || dt->text == NULL) {
        return INV_DT;
    }

    if (dt->isText) {
        // For text DateTime, date and time must be empty strings and UTC must be false
        if (strlen(dt->date) > 0 || strlen(dt->time) > 0 || dt->UTC) {
            return INV_DT;
        }
        // Text must not be empty for text DateTime
        if (strlen(dt->text) == 0) {
            return INV_DT;
        }
    } else {
        // For non-text DateTime, text must be empty string
        if (strlen(dt->text) > 0) {
            return INV_DT;
        }
        // Either date or time must be specified
        if (strlen(dt->date) == 0 && strlen(dt->time) == 0) {
            return INV_DT;
        }
        // If date is specified, validate format (YYYYMMDD)
        if (strlen(dt->date) > 0 && strlen(dt->date) != 8) {
            return INV_DT;
        }
        // If time is specified, validate format (HHMMSS)
        if (strlen(dt->time) > 0 && strlen(dt->time) != 6) {
            return INV_DT;
        }
    }

    return OK;
}

// Helper function to validate a Property object
static VCardErrorCode validateProperty(const Property* prop, bool isOptional) {
    if (prop == NULL) {
        return INV_PROP;
    }

    // Check required fields
    if (prop->name == NULL || prop->group == NULL || 
        prop->parameters == NULL || prop->values == NULL) {
        return INV_PROP;
    }

    // Check name and group constraints
    if (strlen(prop->name) == 0) {
        return INV_PROP;
    }

    // Check for VERSION in optional properties (not allowed)
    if (isOptional && strcmp(prop->name, "VERSION") == 0) {
        return INV_CARD;
    }

    // Validate property name against vCard spec
    if (!isValidPropertyName(prop->name)) {
        return INV_PROP;
    }

    // Validate parameters
    ListIterator paramIter = createIterator(prop->parameters);
    void* elem;
    while ((elem = nextElement(&paramIter)) != NULL) {
        Parameter* param = (Parameter*)elem;
        if (param->name == NULL || param->value == NULL ||
            strlen(param->name) == 0 || strlen(param->value) == 0) {
            return INV_PROP;
        }
    }

    // Validate values
    if (getLength(prop->values) == 0) {
        return INV_PROP;
    }

    // Special validation for N property (must have exactly 5 values)
    if (strcmp(prop->name, "N") == 0 && getLength(prop->values) != 5) {
        return INV_PROP;
    }

    ListIterator valueIter = createIterator(prop->values);
    while ((elem = nextElement(&valueIter)) != NULL) {
        char* value = (char*)elem;
        if (value == NULL) {
            return INV_PROP;
        }
        // Empty strings are allowed for values
    }

    return OK;
}

VCardErrorCode validateCard(const Card* obj) {
    // Check if card is NULL
    if (obj == NULL) {
        return INV_CARD;
    }

    // Check required FN property
    if (obj->fn == NULL) {
        return INV_CARD;
    }

    // Validate FN property
    VCardErrorCode fnResult = validateProperty(obj->fn, false);
    if (fnResult != OK) {
        return fnResult;
    }

    // Check required optionalProperties list
    if (obj->optionalProperties == NULL) {
        return INV_CARD;
    }

    // Track N property occurrence
    bool hasN = false;

    // Validate optional properties
    ListIterator iter = createIterator(obj->optionalProperties);
    void* elem;
    while ((elem = nextElement(&iter)) != NULL) {
        Property* prop = (Property*)elem;
        
        // Check for BDAY/ANNIVERSARY in optional properties (not allowed)
        if (strcmp(prop->name, "BDAY") == 0 || 
            strcmp(prop->name, "ANNIVERSARY") == 0) {
            return INV_DT;  // Changed from INV_CARD to INV_DT
        }

        // Validate each property
        VCardErrorCode propResult = validateProperty(prop, true);
        if (propResult != OK) {
            return propResult;
        }

        // Check N property cardinality
        if (strcmp(prop->name, "N") == 0) {
            if (hasN) {
                return INV_PROP;
            }
            hasN = true;
        }
    }

    // Validate birthday if present
    VCardErrorCode bdayResult = validateDateTime(obj->birthday);
    if (bdayResult != OK) {
        return bdayResult;
    }

    // Validate anniversary if present
    VCardErrorCode annivResult = validateDateTime(obj->anniversary);
    if (annivResult != OK) {
        return annivResult;
    }

    return OK;
} 