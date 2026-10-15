PRAGMA foreign_keys = ON;

-- 1. Members
CREATE TABLE members (
    member_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    class TEXT,
    phone TEXT,
    email TEXT,
    status TEXT NOT NULL DEFAULT 'Active'
        CHECK (status IN ('Active', 'Former')),
    join_date TEXT NOT NULL 
);

-- 2. Formal meetings
CREATE TABLE meetings (
    meeting_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    type TEXT NOT NULL
        CHECK (type IN ('AGM', 'EGM', 'ECM')),
    meeting_date TEXT,
    start_time TEXT,
    end_time TEXT,
    location TEXT,
    CHECK (start_time IS NULL OR end_time IS NULL OR end_time >= start_time)
);

-- 3. Meeting attendance
CREATE TABLE meeting_attendance (
    meeting_id INTEGER NOT NULL,
    member_id INTEGER NOT NULL,
    PRIMARY KEY (meeting_id, member_id),
    FOREIGN KEY (meeting_id)
        REFERENCES meetings(meeting_id) ON DELETE RESTRICT,
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- 4. Projects
CREATE TABLE projects (
    project_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'Planned'
        CHECK (status IN (
            'Planned', 'Ongoing', 'Completed', 'Cancelled'
        )),
    start_date TEXT,
    target_end_date TEXT,
    actual_end_date TEXT,
    CHECK (
        start_date IS NULL
        OR target_end_date IS NULL
        OR target_end_date >= start_date
    ),
    CHECK (
        start_date IS NULL
        OR actual_end_date IS NULL
        OR actual_end_date >= start_date
    )
);

-- 5. Project members
CREATE TABLE project_members (
    project_id INTEGER NOT NULL,
    member_id INTEGER NOT NULL,
    role TEXT NOT NULL DEFAULT 'Member' CHECK (role in ('Member','Lead')),
    PRIMARY KEY (project_id, member_id),
    FOREIGN KEY (project_id)
        REFERENCES projects(project_id) ON DELETE RESTRICT,
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- 6. Committee appointments
CREATE TABLE committee_appointments (
    start_date TEXT NOT NULL,
    office TEXT NOT NULL,
    member_id INTEGER NOT NULL,
    end_date TEXT,
    PRIMARY KEY (start_date, office),
    CHECK (end_date IS NULL OR end_date >= start_date),
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- 7. Society-run activities
CREATE TABLE activities (
    activity_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    type TEXT NOT NULL,
    activity_date TEXT,
    start_time TEXT,
    end_time TEXT,
    location TEXT,
    CHECK (start_time IS NULL OR end_time IS NULL OR end_time >= start_time)
);

-- 8. External competitions
CREATE TABLE external_competitions (
    competition_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    year INTEGER NOT NULL,
    UNIQUE (title, year)
);

-- 9. External competition participants
CREATE TABLE competition_participants (
    competition_id INTEGER NOT NULL,
    member_id INTEGER NOT NULL,
    role TEXT NOT NULL,
    PRIMARY KEY (competition_id, member_id),
    FOREIGN KEY (competition_id)
        REFERENCES external_competitions(competition_id)
        ON DELETE RESTRICT,
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

-- Indexes for common lookups
CREATE INDEX idx_members_status
    ON members(status);

CREATE INDEX idx_attendance_member
    ON meeting_attendance(member_id);

CREATE INDEX idx_project_members_member
    ON project_members(member_id);

CREATE INDEX idx_committee_member
    ON committee_appointments(member_id);

CREATE INDEX idx_competition_participants_member
    ON competition_participants(member_id);
