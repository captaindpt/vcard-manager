# vCard Manager

## Overview

The vCard Manager is a comprehensive application developed as part of the CIS*2750 course at the University of Guelph. It is designed to manage vCard files, providing a seamless interface for creating, editing, and validating vCard data. This project integrates a Python-based user interface with a C library to handle vCard operations, ensuring robust validation and manipulation of vCard files. Additionally, it incorporates database functionality to track and query vCard data efficiently.

## Features

### User Interface

- **Asciimatics-based UI**: The application uses the Asciimatics library to create a text-based user interface, allowing users to interact with vCard files in a pseudo-GUI environment.
- **Main View**: Displays a list of valid vCard files, allowing users to create, edit, or query vCard data.
- **Details View**: Provides detailed information about a selected vCard, including contact name, birthday, anniversary, and other properties. Users can edit the contact name and save changes.
- **Database Query View**: Allows users to execute predefined queries on the vCard database, displaying results in a structured format.

### vCard Operations

- **Validation**: The application validates vCard files against both the internal Card struct specification and the vCard format, ensuring data integrity.
- **Creation and Editing**: Users can create new vCard files or edit existing ones, with changes reflected in both the file system and the database.
- **C Library Integration**: Utilizes a C library for efficient vCard parsing and validation, interfacing with Python through Ctypes.

### Database Functionality

- **MySQL Integration**: Connects to a MySQL database to store and manage vCard data, with tables for files and contacts.
- **Automatic Updates**: Tracks changes to vCard files, updating the database to reflect modifications and new entries.
- **Query Execution**: Supports queries to display all contacts or find contacts born in June, leveraging SQL for data retrieval and sorting.

## Technical Details

- **C Library**: The project includes a C library (`libvcparser.so`) that provides functions for creating, validating, and writing vCard objects. This library is crucial for ensuring the correctness of vCard data.
- **Python-C Integration**: The application uses Ctypes to call C functions from Python, enabling seamless interaction between the UI and the underlying vCard operations.
- **Database Schema**: The database consists of two tables, `FILE` and `CONTACT`, designed to store file metadata and contact information, respectively. Foreign key constraints ensure data consistency.
- **Error Handling**: Comprehensive error handling is implemented across the application, with specific error codes for different validation failures.

## Setup and Execution

1. **Compilation**: Use the provided Makefile to compile the C library. Run `make` in the main directory.
2. **Execution**: Navigate to the `bin` directory and run the application with `python3 A3main.py`.
3. **Database Connection**: The application requires a MySQL database connection. Users must provide their credentials through the Login View.
