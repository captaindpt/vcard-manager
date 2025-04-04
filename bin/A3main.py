#!/usr/bin/env python3

import os
import sys
import ctypes
import mysql.connector
import logging
from datetime import datetime
from asciimatics.screen import Screen
from asciimatics.scene import Scene
from asciimatics.widgets import Frame, Layout, Button, TextBox, Label, ListBox, Text, Widget
from asciimatics.exceptions import NextScene, StopApplication, ResizeScreenError

# Set up logging
log_dir = os.path.join(os.path.dirname(__file__), 'logs')
if not os.path.exists(log_dir):
    os.makedirs(log_dir)

log_file = os.path.join(log_dir, f'vcard_manager_{datetime.now().strftime("%Y%m%d_%H%M%S")}.log')
logging.basicConfig(
    filename=log_file,
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

# Error codes from VCParser.h
class VCardErrorCode:
    OK = 0
    INV_FILE = 1
    INV_CARD = 2
    INV_PROP = 3
    INV_DT = 4
    WRITE_ERROR = 5
    OTHER_ERROR = 6

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

# Load the C library
try:
    lib_path = os.path.join(os.path.dirname(__file__), 'libvcparser.so')
    libvc = ctypes.CDLL(lib_path)
    
    # Set up C function signatures
    libvc.createCard.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.POINTER(Card))]
    libvc.createCard.restype = ctypes.c_int  # VCardErrorCode
    
    libvc.validateCard.argtypes = [ctypes.POINTER(Card)]
    libvc.validateCard.restype = ctypes.c_int  # VCardErrorCode
    
    libvc.writeCard.argtypes = [ctypes.c_char_p, ctypes.POINTER(Card)]
    libvc.writeCard.restype = ctypes.c_int  # VCardErrorCode
    
    libvc.deleteCard.argtypes = [ctypes.POINTER(Card)]
    libvc.deleteCard.restype = None
    
    libvc.cardToString.argtypes = [ctypes.POINTER(Card)]
    libvc.cardToString.restype = ctypes.c_char_p

except OSError as e:
    print(f"Error loading C library: {e}")
    sys.exit(1)

# Wrapper functions for C library
def create_card(filename):
    """Create a Card object from a file."""
    card_ptr = ctypes.POINTER(Card)()
    result = libvc.createCard(filename.encode('utf-8'), ctypes.byref(card_ptr))
    if result != VCardErrorCode.OK:
        return None, result
    return card_ptr, result

def validate_card(card):
    """Validate a Card object."""
    return libvc.validateCard(card)

def write_card(filename, card):
    """Write a Card object to a file."""
    return libvc.writeCard(filename.encode('utf-8'), card)

def delete_card(card):
    """Delete a Card object."""
    libvc.deleteCard(card)

def card_to_string(card):
    """Convert a Card object to string representation."""
    result = libvc.cardToString(card)
    if result:
        return result.decode('utf-8')
    return None

class LoginView(Frame):
    def __init__(self, screen, model):
        super().__init__(
            screen,
            screen.height * 2 // 3,
            screen.width * 2 // 3,
            title="Login to Database",
            reduce_cpu=True
        )
        
        # Save reference to the model
        self.model = model
        
        # Create the form layout
        layout = Layout([1, 18, 1])
        self.add_layout(layout)
        
        # Add widgets
        layout.add_widget(Label("Connect to dursley.socs.uoguelph.ca"), 1)
        layout.add_widget(Label(""), 1)  # Spacer
        
        self._username = Text(
            label="Username:",
            name="username",
            on_change=self._on_change
        )
        layout.add_widget(self._username, 1)
        
        self._password = Text(
            label="Password:",
            name="password",
            hide_char="*",
            on_change=self._on_change
        )
        layout.add_widget(self._password, 1)
        
        self._database = Text(
            label="Database:",
            name="database",
            on_change=self._on_change
        )
        layout.add_widget(self._database, 1)
        
        layout.add_widget(Label(""), 1)  # Spacer
        
        layout2 = Layout([1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 1)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        
        # Add status message area
        layout3 = Layout([1])
        self.add_layout(layout3)
        self._status = Label("", align="^")
        layout3.add_widget(self._status)
        
        self.fix()
    
    def _on_change(self):
        self.save()
    
    def _ok(self):
        self.save()
        username = self.data["username"]
        password = self.data["password"]
        database = self.data["database"]
        
        try:
            # Try to connect to the database
            conn = mysql.connector.connect(
                host="dursley.socs.uoguelph.ca",
                user=username,
                password=password,
                database=database
            )
            
            # Create tables if they don't exist
            cursor = conn.cursor()
            
            # Create FILE table
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS FILE (
                    file_id INT AUTO_INCREMENT PRIMARY KEY,
                    file_name VARCHAR(60) NOT NULL,
                    last_modified DATETIME,
                    creation_time DATETIME NOT NULL
                )
            """)
            
            # Create CONTACT table
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS CONTACT (
                    contact_id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(256) NOT NULL,
                    birthday DATETIME,
                    anniversary DATETIME,
                    file_id INT NOT NULL,
                    FOREIGN KEY (file_id) REFERENCES FILE(file_id) ON DELETE CASCADE
                )
            """)
            
            conn.commit()
            cursor.close()
            conn.close()
            
            # Store connection info in model
            self.model.db_config = {
                "host": "dursley.socs.uoguelph.ca",
                "user": username,
                "password": password,
                "database": database
            }
            
            # Proceed to main view
            raise NextScene("Main")
            
        except mysql.connector.Error as err:
            self._status.text = f"Database Error: {err}"
    
    def _cancel(self):
        # Skip database connection and go to main view
        raise NextScene("Main")

class MainView(Frame):
    def __init__(self, screen, model):
        super().__init__(
            screen,
            screen.height * 2 // 3,
            screen.width * 2 // 3,
            title="vCard Manager",
            reduce_cpu=True
        )
        
        # Save reference to the model
        self.model = model
        
        # Create the layouts
        layout1 = Layout([1], fill_frame=True)
        self.add_layout(layout1)
        
        # Add file list
        self._list_view = ListBox(
            Widget.FILL_FRAME,
            options=[],
            name="files",
            on_change=self._on_pick,
            on_select=self._edit
        )
        layout1.add_widget(self._list_view)
        
        # Add button layout at bottom
        layout2 = Layout([1, 1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Create", self._create), 0)
        layout2.add_widget(Button("Edit", self._edit), 1)
        layout2.add_widget(Button("DB queries", self._db_queries), 2)
        layout2.add_widget(Button("Exit", self._quit), 3)
        
        # Add status message area
        layout3 = Layout([1])
        self.add_layout(layout3)
        self._status = Label("", align="^")
        layout3.add_widget(self._status)
        
        # Fix the layout
        self.fix()
        
        # Populate the list
        self._refresh_list()
    
    def _refresh_list(self):
        """Update the list view with valid cards from the cache."""
        try:
            # Refresh the card cache
            self.model.refresh_card_cache()
            
            # Get valid cards
            valid_cards = self.model.get_valid_cards()
            
            # Update the list view
            self._list_view.options = [(filename, filename) for filename in valid_cards]
            
            # Update status
            if not valid_cards:
                self._status.text = "No valid vCard files found"
                logging.info("No valid vCard files found in directory")
            else:
                self._status.text = f"Found {len(valid_cards)} valid vCard files"
                logging.info(f"Found {len(valid_cards)} valid vCard files")
                
        except Exception as e:
            self._status.text = f"Error reading cards: {str(e)}"
            logging.exception("Error while refreshing card list")
    
    def _on_pick(self):
        """Called when a file is selected in the list."""
        self.save()
        
    def _create(self):
        """Open the Details view to create a new card."""
        raise NextScene("Create")
    
    def _edit(self):
        """Open the Details view to edit the selected card."""
        self.save()
        # Get the selected file
        if self.data["files"]:
            self.model.current_file = self.data["files"]
            raise NextScene("Edit")
    
    def _db_queries(self):
        """Open the DB Query view."""
        raise NextScene("Query")
    
    def _quit(self):
        raise StopApplication("User requested exit")
    
    @property
    def frame_update_count(self):
        # Refresh the list every 10 frames
        return 10
    
    def _update(self, frame_no):
        # Refresh the list periodically
        super()._update(frame_no)
        if frame_no % self.frame_update_count == 0:
            self._refresh_list()

class CreateCardView(Frame):
    def __init__(self, screen, model):
        super().__init__(
            screen,
            screen.height * 2 // 3,
            screen.width * 2 // 3,
            title="Create New vCard",
            reduce_cpu=True
        )
        
        # Save reference to the model
        self.model = model
        
        # Create the form layout
        layout = Layout([1, 18, 1])
        self.add_layout(layout)
        
        # Add widgets - only filename and contact name are editable
        self._filename = Text(
            label="File Name:",
            name="filename",
            on_change=self._on_change
        )
        layout.add_widget(self._filename, 1)
        
        self._contact_name = Text(
            label="Contact Name:",
            name="contact_name",
            on_change=self._on_change
        )
        layout.add_widget(self._contact_name, 1)
        
        # Add read-only placeholders
        layout.add_widget(Label("Birthday: Not available for new cards"), 1)
        layout.add_widget(Label("Anniversary: Not available for new cards"), 1)
        layout.add_widget(Label("Other Properties: Not available for new cards"), 1)
        
        layout.add_widget(Label(""), 1)  # Spacer
        
        # Add buttons
        layout2 = Layout([1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 1)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        
        # Add status message area
        layout3 = Layout([1])
        self.add_layout(layout3)
        self._status = Label("", align="^")
        layout3.add_widget(self._status)
        
        # Initialize empty form
        self.data = {
            "filename": "",
            "contact_name": ""
        }
        
        self.fix()
    
    def _on_change(self):
        self.save()
        logging.debug("Form field changed")
    
    def _ok(self):
        self.save()
        logging.debug("CreateCardView._ok called")
        
        try:
            # Validate inputs
            if not self.data["contact_name"].strip():
                self._status.text = "Contact name cannot be empty"
                logging.warning("Empty contact name provided")
                return
            
            if not self.data["filename"].strip():
                self._status.text = "File name cannot be empty"
                logging.warning("Empty filename provided")
                return
            
            # Add .vcf extension if missing
            if not self.data["filename"].endswith('.vcf'):
                self.data["filename"] += '.vcf'
                logging.debug(f"Added .vcf extension to filename: {self.data['filename']}")
            
            filepath = os.path.abspath(os.path.join(self.model.cards_dir, self.data["filename"]))
            logging.debug(f"Full filepath for new card: {filepath}")
            
            if os.path.exists(filepath):
                self._status.text = "File already exists"
                logging.warning(f"File already exists: {filepath}")
                return
            
            try:
                # Create minimal vCard content
                vcard_content = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:{}\r\nEND:VCARD\r\n".format(
                    self.data["contact_name"].strip()
                )
                logging.debug(f"Generated vCard content:\n{vcard_content}")
                
                # Write the initial vCard file
                with open(filepath, 'w', newline='') as f:
                    f.write(vcard_content)
                logging.info(f"Written vCard file: {filepath}")
                
                # Create and validate the card
                card, result = create_card(filepath)
                logging.debug(f"create_card result: {result}")
                
                if result != VCardErrorCode.OK:
                    os.remove(filepath)
                    self._status.text = f"Error creating card: Invalid format (code {result})"
                    logging.error(f"Card creation failed with code {result}")
                    return
                
                # Update database if connected
                if self.model.db_config:
                    try:
                        conn = mysql.connector.connect(**self.model.db_config)
                        cursor = conn.cursor()
                        
                        # Insert into FILE table
                        cursor.execute("""
                            INSERT INTO FILE (file_name, creation_time, last_modified)
                            VALUES (%s, NOW(), NOW())
                        """, (self.data["filename"],))
                        
                        # Get the file_id
                        file_id = cursor.lastrowid
                        
                        # Insert into CONTACT table
                        cursor.execute("""
                            INSERT INTO CONTACT (name, file_id)
                            VALUES (%s, %s)
                        """, (self.data["contact_name"], file_id))
                        
                        conn.commit()
                        cursor.close()
                        conn.close()
                        logging.info("Database updated successfully")
                    except mysql.connector.Error as err:
                        self._status.text = f"Database Error: {err}"
                        logging.error(f"Database error: {err}")
                        return
                
                delete_card(card)
                logging.info("Card created successfully")
                raise NextScene("Main")
                
            except Exception as e:
                self._status.text = f"Error creating card: {str(e)}"
                logging.exception("Unexpected error during card creation")
                return
            
        except NextScene:
            raise
        except Exception as e:
            self._status.text = f"Unexpected error: {str(e)}"
            logging.exception("Unexpected error in _ok method")
            return
    
    def _cancel(self):
        raise NextScene("Main")

class EditCardView(Frame):
    def __init__(self, screen, model):
        super().__init__(
            screen,
            screen.height * 2 // 3,
            screen.width * 2 // 3,
            title="Edit vCard",
            reduce_cpu=True
        )
        
        # Save reference to the model
        self.model = model
        
        # Create the form layout
        layout = Layout([1, 18, 1])
        self.add_layout(layout)
        
        # Add widgets
        self._filename = Text(
            label="File Name:",
            name="filename",
            readonly=True  # Always readonly in edit mode
        )
        layout.add_widget(self._filename, 1)
        
        self._contact_name = Text(
            label="Contact Name:",
            name="contact_name",
            on_change=self._on_change
        )
        layout.add_widget(self._contact_name, 1)
        
        self._birthday = Text(
            label="Birthday:",
            name="birthday",
            readonly=True
        )
        layout.add_widget(self._birthday, 1)
        
        self._anniversary = Text(
            label="Anniversary:",
            name="anniversary",
            readonly=True
        )
        layout.add_widget(self._anniversary, 1)
        
        self._other_props = Text(
            label="Other Properties:",
            name="other_props",
            readonly=True
        )
        layout.add_widget(self._other_props, 1)
        
        layout.add_widget(Label(""), 1)  # Spacer
        
        # Add buttons
        layout2 = Layout([1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 1)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        
        # Add status message area
        layout3 = Layout([1])
        self.add_layout(layout3)
        self._status = Label("", align="^")
        layout3.add_widget(self._status)
        
        self.fix()
        
        # Initialize empty data
        self.data = {
            "filename": "",
            "contact_name": "",
            "birthday": "",
            "anniversary": "",
            "other_props": ""
        }
    
    def reset(self):
        # This is called when the scene becomes active
        super().reset()
        
        # Check if we have a file to edit
        if not hasattr(self.model, 'current_file'):
            raise NextScene("Main")
        
        # Load the card data
        self._load_card()
    
    def _extract_fn_value(self, card):
        """Extract the FN (formatted name) value from a card."""
        try:
            if not card or not card.contents:
                logging.warning("Card or card contents is None")
                return ""
                
            if not hasattr(card.contents, 'fn') or not card.contents.fn:
                logging.warning("Card has no FN property")
                return ""
                
            fn_prop = card.contents.fn
            if not fn_prop:
                logging.warning("FN property is None")
                return ""
                
            if not fn_prop.contents or not fn_prop.contents.values:
                logging.warning("FN property has no values")
                return ""
                
            values_list = fn_prop.contents.values.contents
            if not values_list or not values_list.head:
                logging.warning("Values list is empty")
                return ""
                
            # Get the first value node
            head_node = values_list.head
            if not head_node:
                logging.warning("Head node is None")
                return ""
                
            # Cast the data pointer to char*
            try:
                value = ctypes.cast(head_node.contents.data, ctypes.c_char_p)
                if value and value.value:
                    return value.value.decode('utf-8')
            except (AttributeError, ValueError) as e:
                logging.error(f"Error casting FN value: {e}")
                return ""
                
            return ""
        except Exception as e:
            logging.exception("Error extracting FN value")
            return ""
    
    def _format_datetime(self, dt):
        """Format a DateTime structure into a readable string."""
        if not dt:
            return "Not specified"
            
        if dt.contents.isText:
            return dt.contents.text.decode('utf-8') if dt.contents.text else "Not specified"
            
        date_str = dt.contents.date.decode('utf-8') if dt.contents.date else ""
        time_str = dt.contents.time.decode('utf-8') if dt.contents.time else ""
        
        if not date_str and not time_str:
            return "Not specified"
            
        result = date_str
        if time_str:
            result += f" {time_str}"
        if dt.contents.UTC:
            result += " UTC"
        return result
    
    def _count_optional_properties(self, card):
        """Count the number of optional properties in the card."""
        if not card or not card.contents.optionalProperties:
            return 0
            
        return card.contents.optionalProperties.contents.length
    
    def _load_card(self):
        """Load the card data into the form."""
        if not hasattr(self.model, 'current_file'):
            raise NextScene("Main")
        
        logging.debug("Loading card data for editing")
        filepath = os.path.join(self.model.cards_dir, self.model.current_file)
        card, result = create_card(filepath)
        
        if result == VCardErrorCode.OK:
            try:
                # Get the formatted name (FN property)
                fn_value = self._extract_fn_value(card)
                logging.debug(f"Extracted FN value: {fn_value}")
                
                # Format birthday if present
                birthday_str = "Not specified"
                if card.contents.birthday:
                    birthday_str = self._format_datetime(card.contents.birthday)
                logging.debug(f"Formatted birthday: {birthday_str}")
                
                # Format anniversary if present
                anniversary_str = "Not specified"
                if card.contents.anniversary:
                    anniversary_str = self._format_datetime(card.contents.anniversary)
                logging.debug(f"Formatted anniversary: {anniversary_str}")
                
                # Count optional properties
                opt_props_count = self._count_optional_properties(card)
                logging.debug(f"Optional properties count: {opt_props_count}")
                
                # Set form data
                self.data = {
                    "filename": self.model.current_file,
                    "contact_name": fn_value,
                    "birthday": birthday_str,
                    "anniversary": anniversary_str,
                    "other_props": f"{opt_props_count} additional properties"
                }
                logging.debug("Card data loaded into form")
                
            except Exception as e:
                logging.exception("Error extracting card data")
                self._status.text = "Error loading card data"
                raise NextScene("Main")
            finally:
                delete_card(card)
        else:
            logging.error(f"Failed to load card: {result}")
            self._status.text = "Error loading card"
            raise NextScene("Main")
    
    def _on_change(self):
        self.save()
        logging.debug("Form field changed")
    
    def _ok(self):
        self.save()
        logging.debug("EditCardView._ok called")
        
        try:
            # Validate inputs
            if not self.data["contact_name"].strip():
                self._status.text = "Contact name cannot be empty"
                logging.warning("Empty contact name provided")
                return
            
            filepath = os.path.join(self.model.cards_dir, self.model.current_file)
            logging.debug(f"Attempting to edit card: {filepath}")
            
            # Load the existing card
            card, result = create_card(filepath)
            if result != VCardErrorCode.OK:
                self._status.text = "Error loading card"
                logging.error(f"Error loading card for editing, code: {result}")
                return
            
            try:
                # Create new content with updated FN
                vcard_content = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:{}\r\nEND:VCARD\r\n".format(
                    self.data["contact_name"].strip()
                )
                
                # Write the updated card
                with open(filepath, 'w', newline='') as f:
                    f.write(vcard_content)
                logging.info(f"Updated vCard file: {filepath}")
                
                # Validate the updated card
                new_card, new_result = create_card(filepath)
                if new_result != VCardErrorCode.OK:
                    self._status.text = "Error saving changes"
                    logging.error(f"Error validating updated card, code: {new_result}")
                    return
                
                # Update database if connected
                if self.model.db_config:
                    try:
                        conn = mysql.connector.connect(**self.model.db_config)
                        cursor = conn.cursor()
                        
                        # Update CONTACT table
                        cursor.execute("""
                            UPDATE CONTACT c
                            JOIN FILE f ON c.file_id = f.file_id
                            SET c.name = %s,
                                f.last_modified = NOW()
                            WHERE f.file_name = %s
                        """, (self.data["contact_name"], self.model.current_file))
                        
                        conn.commit()
                        cursor.close()
                        conn.close()
                        logging.info("Database updated successfully")
                    except mysql.connector.Error as err:
                        self._status.text = f"Database Error: {err}"
                        logging.error(f"Database error: {err}")
                        return
                
                delete_card(new_card)
                logging.info("Card updated successfully")
                raise NextScene("Main")
                
            except Exception as e:
                self._status.text = f"Error updating card: {str(e)}"
                logging.exception("Error during card update")
                return
            
        except NextScene:
            raise
        except Exception as e:
            self._status.text = f"Unexpected error: {str(e)}"
            logging.exception("Unexpected error in _ok method")
            return
    
    def _cancel(self):
        raise NextScene("Main")

class DBQueryView(Frame):
    def __init__(self, screen, model):
        super().__init__(
            screen,
            screen.height * 2 // 3,
            screen.width * 2 // 3,
            title="Database Queries",
            reduce_cpu=True
        )
        
        # Save reference to the model
        self.model = model
        
        # Create the main layout for results
        layout1 = Layout([100], fill_frame=True)
        self.add_layout(layout1)
        
        # Add results display area with proper sizing
        self._results = TextBox(
            Widget.FILL_FRAME,
            name="results",
            readonly=True,
            as_string=True,
            line_wrap=True
        )
        layout1.add_widget(self._results)
        
        # Add button layout at bottom with proper spacing
        layout2 = Layout([1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Display all contacts", self._display_all), 0)
        layout2.add_widget(Button("Find contacts born in June", self._find_june), 1)
        layout2.add_widget(Button("Cancel", self._cancel), 2)
        
        # Add status message area
        layout3 = Layout([100])
        self.add_layout(layout3)
        self._status = Label("", align="^")
        layout3.add_widget(self._status)
        
        self.fix()
    
    def _display_all(self):
        """Display all contacts with their file information."""
        if not self.model.db_config:
            self._status.text = "No database connection"
            return
            
        try:
            conn = mysql.connector.connect(**self.model.db_config)
            cursor = conn.cursor()
            
            # Execute query to get all contacts with file info
            cursor.execute("""
                SELECT c.name, f.file_name, 
                       COALESCE(DATE_FORMAT(c.birthday, '%Y-%m-%d'), 'Not specified') as birthday,
                       COALESCE(DATE_FORMAT(c.anniversary, '%Y-%m-%d'), 'Not specified') as anniversary
                FROM CONTACT c
                JOIN FILE f ON c.file_id = f.file_id
                ORDER BY c.name, f.file_name
            """)
            
            results = cursor.fetchall()
            
            # Format results
            if not results:
                self._results.value = "No contacts found in database."
            else:
                # Define column widths
                name_width = 25
                file_width = 20
                date_width = 15
                
                # Create the header
                output = "All Contacts:\n\n"
                output += "{:<{nw}} {:<{fw}} {:<{dw}} {:<{dw}}\n".format(
                    "Name", "File", "Birthday", "Anniversary",
                    nw=name_width, fw=file_width, dw=date_width
                )
                output += "-" * (name_width + file_width + date_width * 2 + 3) + "\n"
                
                # Format each row
                for row in results:
                    name, filename, birthday, anniversary = row
                    output += "{:<{nw}} {:<{fw}} {:<{dw}} {:<{dw}}\n".format(
                        name, filename, birthday, anniversary,
                        nw=name_width, fw=file_width, dw=date_width
                    )
                
                # Add database statistics
                cursor.execute("SELECT COUNT(*) FROM FILE")
                file_count = cursor.fetchone()[0]
                cursor.execute("SELECT COUNT(*) FROM CONTACT")
                contact_count = cursor.fetchone()[0]
                
                output += f"\nDatabase has {file_count} files and {contact_count} contacts."
                
                self._results.value = output
            
            cursor.close()
            conn.close()
            
        except mysql.connector.Error as err:
            self._status.text = f"Database Error: {err}"
            logging.error(f"Database error in display_all: {err}")
    
    def _find_june(self):
        """Find contacts born in June."""
        if not self.model.db_config:
            self._status.text = "No database connection"
            return
            
        try:
            conn = mysql.connector.connect(**self.model.db_config)
            cursor = conn.cursor()
            
            # Execute query to find contacts born in June
            cursor.execute("""
                SELECT c.name, f.file_name, 
                       DATE_FORMAT(c.birthday, '%Y-%m-%d') as birthday
                FROM CONTACT c
                JOIN FILE f ON c.file_id = f.file_id
                WHERE MONTH(c.birthday) = 6
                ORDER BY DAY(c.birthday), c.name
            """)
            
            results = cursor.fetchall()
            
            # Format results
            if not results:
                self._results.value = "No contacts with June birthdays found."
            else:
                # Define column widths
                name_width = 25
                file_width = 20
                date_width = 15
                
                # Create the header
                output = "Contacts Born in June:\n\n"
                output += "{:<{nw}} {:<{fw}} {:<{dw}}\n".format(
                    "Name", "File", "Birthday",
                    nw=name_width, fw=file_width, dw=date_width
                )
                output += "-" * (name_width + file_width + date_width + 2) + "\n"
                
                # Format each row
                for row in results:
                    name, filename, birthday = row
                    output += "{:<{nw}} {:<{fw}} {:<{dw}}\n".format(
                        name, filename, birthday,
                        nw=name_width, fw=file_width, dw=date_width
                    )
                
                self._results.value = output
            
            cursor.close()
            conn.close()
            
        except mysql.connector.Error as err:
            self._status.text = f"Database Error: {err}"
            logging.error(f"Database error in find_june: {err}")
    
    def _cancel(self):
        raise NextScene("Main")

class Model:
    def __init__(self):
        # Database configuration
        self.db_config = None
        
        # Current working directory
        self.cards_dir = os.path.join(os.path.dirname(__file__), 'cards')
        
        # Ensure cards directory exists
        if not os.path.exists(self.cards_dir):
            os.makedirs(self.cards_dir)
            
        # Initialize card cache
        self.card_cache = {}  # filename -> (card_ptr, last_modified)
        self.refresh_card_cache()
    
    def refresh_card_cache(self):
        """Scan directory and update card cache."""
        logging.info("Refreshing card cache")
        current_files = set()
        
        try:
            # Log the cards directory path
            logging.info(f"Scanning directory: {self.cards_dir}")
            
            # Get all files in directory
            all_files = os.listdir(self.cards_dir)
            logging.info(f"All files in directory: {all_files}")
            
            # Scan all .vcf files
            for filename in all_files:
                if not filename.endswith('.vcf'):
                    logging.debug(f"Skipping non-vcf file: {filename}")
                    continue
                    
                filepath = os.path.join(self.cards_dir, filename)
                current_files.add(filename)
                logging.info(f"Found vCard file: {filename}")
                
                # Log file details
                file_stat = os.stat(filepath)
                logging.info(f"File details for {filename}:")
                logging.info(f"  Size: {file_stat.st_size} bytes")
                logging.info(f"  Permissions: {oct(file_stat.st_mode)}")
                logging.info(f"  Modified: {datetime.fromtimestamp(file_stat.st_mtime)}")
                
                # Check if file was modified since last cache
                last_modified = os.path.getmtime(filepath)
                if (filename in self.card_cache and 
                    self.card_cache[filename][1] == last_modified):
                    logging.info(f"Card {filename} unchanged, using cached version")
                    continue  # File hasn't changed, skip processing
                
                # Create and validate card
                try:
                    logging.info(f"Processing card: {filename}")
                    
                    # Log file contents for debugging
                    with open(filepath, 'r') as f:
                        content = f.read()
                        logging.info(f"File contents of {filename}:\n{content}")
                    
                    card_ptr, result = create_card(filepath)
                    
                    # Log detailed error for card creation
                    if result != VCardErrorCode.OK:
                        error_msg = {
                            VCardErrorCode.INV_FILE: "Invalid file",
                            VCardErrorCode.INV_CARD: "Invalid card format",
                            VCardErrorCode.INV_PROP: "Invalid property",
                            VCardErrorCode.INV_DT: "Invalid date/time",
                            VCardErrorCode.WRITE_ERROR: "Write error",
                            VCardErrorCode.OTHER_ERROR: "Other error"
                        }.get(result, "Unknown error")
                        logging.error(f"Failed to create card {filename}: {error_msg} (code {result})")
                        if filename in self.card_cache:
                            del self.card_cache[filename]
                        continue
                    
                    logging.info(f"Successfully created card for {filename}")
                    
                    # Validate the card
                    val_result = validate_card(card_ptr)
                    
                    # Log detailed error for validation
                    if val_result != VCardErrorCode.OK:
                        error_msg = {
                            VCardErrorCode.INV_FILE: "Invalid file",
                            VCardErrorCode.INV_CARD: "Invalid card format",
                            VCardErrorCode.INV_PROP: "Invalid property",
                            VCardErrorCode.INV_DT: "Invalid date/time",
                            VCardErrorCode.WRITE_ERROR: "Write error",
                            VCardErrorCode.OTHER_ERROR: "Other error"
                        }.get(val_result, "Unknown error")
                        logging.error(f"Card validation failed for {filename}: {error_msg} (code {val_result})")
                        delete_card(card_ptr)
                        if filename in self.card_cache:
                            del self.card_cache[filename]
                        continue
                    
                    logging.info(f"Successfully validated card for {filename}")
                    
                    # Cache the valid card
                    if filename in self.card_cache:
                        # Delete old card pointer
                        old_card_ptr = self.card_cache[filename][0]
                        if old_card_ptr:
                            delete_card(old_card_ptr)
                            logging.info(f"Deleted old card pointer for {filename}")
                    
                    self.card_cache[filename] = (card_ptr, last_modified)
                    logging.info(f"Successfully cached valid card: {filename}")
                    
                except Exception as e:
                    logging.exception(f"Error processing card {filename}: {str(e)}")
                    if filename in self.card_cache:
                        del self.card_cache[filename]
            
            # Log cache state
            logging.info(f"Current cache state:")
            for filename, (card_ptr, mtime) in self.card_cache.items():
                logging.info(f"  {filename}: last_modified={datetime.fromtimestamp(mtime)}")
            
            # Remove cached cards that no longer exist
            for filename in list(self.card_cache.keys()):
                if filename not in current_files:
                    logging.info(f"Removing deleted card from cache: {filename}")
                    card_ptr = self.card_cache[filename][0]
                    if card_ptr:
                        delete_card(card_ptr)
                    del self.card_cache[filename]
            
            logging.info(f"Card cache refresh complete. Valid cards: {list(self.card_cache.keys())}")
                    
        except Exception as e:
            logging.exception(f"Error refreshing card cache: {str(e)}")
    
    def get_valid_cards(self):
        """Return list of valid card filenames."""
        return sorted(self.card_cache.keys())
    
    def get_card(self, filename):
        """Get card pointer from cache."""
        if filename in self.card_cache:
            return self.card_cache[filename][0]
        return None
    
    def update_card(self, filename):
        """Force update of specific card in cache."""
        if filename in self.card_cache:
            filepath = os.path.join(self.cards_dir, filename)
            if os.path.exists(filepath):
                # Delete old card
                old_card_ptr = self.card_cache[filename][0]
                if old_card_ptr:
                    delete_card(old_card_ptr)
                
                # Create and validate new card
                card_ptr, result = create_card(filepath)
                if result == VCardErrorCode.OK:
                    val_result = validate_card(card_ptr)
                    if val_result == VCardErrorCode.OK:
                        self.card_cache[filename] = (card_ptr, os.path.getmtime(filepath))
                        return True
                    delete_card(card_ptr)
            del self.card_cache[filename]
        return False
    
    def __del__(self):
        """Cleanup card cache on deletion."""
        for card_ptr, _ in self.card_cache.values():
            if card_ptr:
                delete_card(card_ptr)

def demo(screen, model):
    scenes = [
        Scene([LoginView(screen, model)], -1, name="Login"),
        Scene([MainView(screen, model)], -1, name="Main"),
        Scene([CreateCardView(screen, model)], -1, name="Create"),
        Scene([EditCardView(screen, model)], -1, name="Edit"),
        Scene([DBQueryView(screen, model)], -1, name="Query")
    ]
    
    screen.play(scenes, stop_on_resize=False, start_scene=scenes[0])

def main():
    model = Model()
    while True:
        try:
            Screen.wrapper(lambda screen: demo(screen, model))
            break
        except ResizeScreenError:
            pass
        except KeyboardInterrupt:
            break

if __name__ == "__main__":
    main() 