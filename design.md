Here's is a structure that I like with the type of view table or form in brackets. Tables are view only btw. Forms are used to edit.
Members
|-- All Members (Table)
|-- Active Members (Table)
|-- New / Edit Member (Form)

Meetings
|-- Annual General Meetings (Table)
|-- Extraordinary General Meetings (Table)
|-- Executive Committee Meetings (Table)
|-- Attendees (Table - use < > icons or meeting id text box in bottom right to go through meetings; display Memberid, name, phone, email table for attendess)
|-- New / Edit Meeting (Form)
|-- Attendance Register (Form - where u can scroll and select the members from list and the select the meeting)

Projects
|-- All Projects (Table)
|-- Ongoing Projects (Table)
|-- Completed Projects (Table)
|-- Project Teams (Table - use < > or project id text box; show memberid, name, phone, email, role)
|-- New / Edit Project (Form)
|-- New / Edit Project Members (Form)

Committee 
|-- All appointments (Table)
|-- Current Committee (Table)
|-- New / Edit Appointment (Form)

Activities
|-- All Activities (Table - use drop down menu in bottom to filter by type)
|-- New / Edit Activity (Form)

External Competition
|-- All competitions (Table) 
|-- New / edit competitions (Form)
|-- Competition Teams (table)
|-- New / edit Competition Team Assignments
