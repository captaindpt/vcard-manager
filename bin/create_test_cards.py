#!/usr/bin/env python3

import os
import sys
import ctypes
from datetime import datetime

# Python classes mirroring C structures
class DateTime(ctypes.Structure):
    _fields_ = [
        ("UTC", ctypes.c_bool),
        ("isText", ctypes.c_bool),
        ("date", ctypes.c_char_p),
        ("time", ctypes.c_char_p),
        ("text", ctypes.c_char_p)
    ]

class Parameter(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("value", ctypes.c_char_p)
    ]

class List(ctypes.Structure):
    _fields_ = [
        ("head", ctypes.c_void_p),
        ("tail", ctypes.c_void_p),
        ("length", ctypes.c_int),
        ("deleteData", ctypes.CFUNCTYPE(None, ctypes.c_void_p)),
        ("compare", ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)),
        ("printData", ctypes.CFUNCTYPE(ctypes.c_char_p, ctypes.c_void_p))
    ]

class Property(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("group", ctypes.c_char_p),
        ("parameters", ctypes.POINTER(List)),
        ("values", ctypes.POINTER(List))
    ]

class Card(ctypes.Structure):
    _fields_ = [
        ("fn", ctypes.POINTER(Property)),
        ("optionalProperties", ctypes.POINTER(List)),
        ("birthday", ctypes.POINTER(DateTime)),
        ("anniversary", ctypes.POINTER(DateTime))
    ]

def create_test_cards():
    # Load the C library
    try:
        lib_path = os.path.join(os.path.dirname(__file__), 'libvcparser.so')
        libvc = ctypes.CDLL(lib_path)
        
        # Set up C function signatures
        libvc.createCard.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.POINTER(Card))]
        libvc.createCard.restype = ctypes.c_int
        
        libvc.writeCard.argtypes = [ctypes.c_char_p, ctypes.POINTER(Card)]
        libvc.writeCard.restype = ctypes.c_int
        
        libvc.deleteCard.argtypes = [ctypes.POINTER(Card)]
        libvc.deleteCard.restype = None
        
        # Create cards directory if it doesn't exist
        cards_dir = os.path.join(os.path.dirname(__file__), 'cards')
        if not os.path.exists(cards_dir):
            os.makedirs(cards_dir)
        
        # Test card 1: Alice Johnson
        alice_content = b'BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Alice Johnson\r\nBDAY:19900615T143000\r\nEND:VCARD\r\n'
        alice_file = os.path.join(cards_dir, 'june_birthday.vcf')
        with open(alice_file, 'wb') as f:
            f.write(alice_content)
        
        # Test card 2: Bob Smith
        bob_content = b'BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Bob Smith\r\nBDAY:19850603T102000\r\nEND:VCARD\r\n'
        bob_file = os.path.join(cards_dir, 'another_june.vcf')
        with open(bob_file, 'wb') as f:
            f.write(bob_content)
        
        # Validate and rewrite both cards using the C library
        for filename in ['june_birthday.vcf', 'another_june.vcf']:
            filepath = os.path.join(cards_dir, filename)
            card_ptr = ctypes.POINTER(Card)()
            result = libvc.createCard(filepath.encode('utf-8'), ctypes.byref(card_ptr))
            
            if result == 0:  # VCardErrorCode.OK
                # Write back using the C library to ensure proper formatting
                write_result = libvc.writeCard(filepath.encode('utf-8'), card_ptr)
                if write_result == 0:
                    print(f"Successfully created and validated {filename}")
                else:
                    print(f"Error writing {filename}: {write_result}")
                libvc.deleteCard(card_ptr)
            else:
                print(f"Error creating {filename}: {result}")
        
        print("\nTest cards have been created successfully!")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    create_test_cards() 