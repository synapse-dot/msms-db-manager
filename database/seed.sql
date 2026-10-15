
PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- 1. Members
INSERT INTO members
    (name, class, phone, email, status, join_date)
VALUES
    ('Alex Morgan', '10', '0770001001',
     'alex.morgan@example.org', 'Active', '2026-01-15'),

    ('Jamie Carter', '10', '0770001002',
     'jamie.carter@example.org', 'Active', '2026-01-15'),

    ('Taylor Reed', '11', '0770001003',
     'taylor.reed@example.org', 'Active', '2026-01-15'),

    ('Jordan Blake', '11', '0770001004',
     'jordan.blake@example.org', 'Active', '2026-01-20'),

    ('Casey Brooks', '9', '0770001005',
     'casey.brooks@example.org', 'Active', '2026-02-01'),

    ('Riley Bennett', '10', NULL,
     'riley.bennett@example.org', 'Active', '2026-02-01'),

    ('Morgan Ellis', '11', '0770001007',
     NULL, 'Former', '2025-01-10'),

    ('Avery Parker', '9', NULL,
     NULL, 'Active', '2026-03-01'),

    ('Cameron Hayes', '10', '0770001009',
     'cameron.hayes@example.org', 'Active', '2026-03-10'),

    ('Drew Foster', '11', '0770001010',
     'drew.foster@example.org', 'Active', '2026-04-05');


-- 2. Formal meetings
INSERT INTO meetings
    (title, type, meeting_date, start_time, end_time, location)
VALUES
    ('Annual General Meeting 2026', 'AGM',
     '2026-01-15', '14:00', '15:30',
     'College Science Laboratory'),

    ('Competition Planning Meeting', 'ECM',
     '2026-02-10', '13:30', '14:15',
     'Computer Laboratory'),

    ('Constitution Review Meeting', 'EGM',
     '2026-03-05', '14:00', '15:00',
     'Classroom 10A'),

    ('Project Progress Review', 'ECM',
     '2026-04-12', '13:45', '14:30',
     'Computer Laboratory'),

    ('Term-End Committee Meeting', 'ECM',
     '2026-06-20', '09:00', '10:00',
     'College Science Laboratory');


-- 3. Meeting attendance
INSERT INTO meeting_attendance (meeting_id, member_id)
VALUES
    (1, 1), (1, 2), (1, 3), (1, 4), (1, 5),
    (1, 6), (1, 8), (1, 9),

    (2, 1), (2, 3), (2, 4), (2, 6),

    (3, 1), (3, 2), (3, 4), (3, 5), (3, 9),

    (4, 1), (4, 3), (4, 5), (4, 8), (4, 10),

    (5, 1), (5, 2), (5, 3), (5, 4), (5, 9), (5, 10);


-- 4. Projects
INSERT INTO projects
    (title, status, start_date, target_end_date, actual_end_date)
VALUES
    ('MSMS Membership Management Application', 'Ongoing',
     '2026-01-20', '2026-12-15', NULL),

    ('School Population Simulation', 'Planned',
     NULL, '2026-12-01', NULL),

    ('Numerical Methods Demonstration', 'Completed',
     '2026-02-01', '2026-03-15', '2026-03-12'),

    ('Traffic Flow Modelling Project', 'Cancelled',
     '2026-03-01', '2026-05-31', NULL),

    ('Competition Demonstration Models', 'Ongoing',
     '2026-04-10', '2026-11-30', NULL);


-- 5. Project members
INSERT INTO project_members (project_id, member_id, role)
VALUES
    (1, 1, 'Lead'),
    (1, 2, 'Member'),
    (1, 3, 'Member'),
    (1, 6, 'Member'),

    (2, 3, 'Lead'),
    (2, 4, 'Member'),
    (2, 8, 'Member'),

    (3, 2, 'Lead'),
    (3, 5, 'Member'),
    (3, 9, 'Member'),

    (4, 4, 'Lead'),
    (4, 6, 'Member'),

    (5, 9, 'Lead'),
    (5, 10, 'Member'),
    (5, 5, 'Member');


-- 6. Committee appointments
INSERT INTO committee_appointments
    (start_date, office, member_id, end_date)
VALUES
    ('2025-01-10', 'President', 7, '2026-01-14'),
    ('2025-01-10', 'Secretary', 2, '2026-01-14'),

    ('2026-01-15', 'President', 1, NULL),
    ('2026-01-15', 'Secretary', 2, NULL),
    ('2026-01-15', 'Treasurer', 3, NULL),
    ('2026-01-15', 'Vice President', 4, NULL);


-- 7. Society-run activities
INSERT INTO activities
    (title, type, activity_date, start_time, end_time, location)
VALUES
    ('Introduction to Simulation and Modelling', 'Workshop',
     '2026-01-22', '14:00', '15:00',
     'Computer Laboratory'),

    ('MSMS Modelling Competition', 'Competition',
     '2026-02-20', '13:00', '15:00',
     'College Hall'),

    ('Open Coding Session: C++ Fundamentals', 'Open Coding Session',
     '2026-03-10', '14:00', '15:30',
     'Computer Laboratory'),

    ('Student Project Demonstrations', 'Presentation',
     '2026-04-20', '13:30', '15:00',
     'Science Laboratory'),

    ('Introduction for New Members', 'Outreach',
     '2026-05-05', NULL, NULL,
     NULL),

    ('Modelling Workshop: Population Growth', 'Workshop',
     '2026-06-12', '14:00', '15:30',
     'Science Laboratory');


-- 8. External competitions
INSERT INTO external_competitions (title, year)
VALUES
    ('National Science Competition', 2026),
    ('Inter-School Programming Challenge', 2026),
    ('National Science Competition', 2025),
    ('Young Innovators Challenge', 2026);


-- 9. External competition participants
INSERT INTO competition_participants
    (competition_id, member_id, role)
VALUES
    (1, 1, 'Participant'),
    (1, 3, 'Participant'),
    (1, 9, 'Participant'),

    (2, 2, 'Participant'),
    (2, 4, 'Participant'),
    (2, 5, 'Participant'),

    (3, 7, 'Participant'),

    (4, 3, 'Participant'),
    (4, 8, 'Participant'),
    (4, 10, 'Participant');

COMMIT;
