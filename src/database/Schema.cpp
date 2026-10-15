#include "Schema.h"

namespace MSMS {

static const QString FULL_SCHEMA_SQL = QString::fromUtf8(R"SQL(
PRAGMA foreign_keys = ON;

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

CREATE TABLE meeting_attendance (
    meeting_id INTEGER NOT NULL,
    member_id INTEGER NOT NULL,
    PRIMARY KEY (meeting_id, member_id),
    FOREIGN KEY (meeting_id)
        REFERENCES meetings(meeting_id) ON DELETE RESTRICT,
    FOREIGN KEY (member_id)
        REFERENCES members(member_id) ON DELETE RESTRICT
);

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

CREATE TABLE external_competitions (
    competition_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    year INTEGER NOT NULL,
    UNIQUE (title, year)
);

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
)SQL");

const QStringList& Schema::tableCreationStatements()
{
    static const QStringList statements = {
        QString::fromUtf8(R"SQL(
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
        )SQL"),
        QString::fromUtf8(R"SQL(
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
        )SQL"),
        QString::fromUtf8(R"SQL(
            CREATE TABLE meeting_attendance (
                meeting_id INTEGER NOT NULL,
                member_id INTEGER NOT NULL,
                PRIMARY KEY (meeting_id, member_id),
                FOREIGN KEY (meeting_id)
                    REFERENCES meetings(meeting_id) ON DELETE RESTRICT,
                FOREIGN KEY (member_id)
                    REFERENCES members(member_id) ON DELETE RESTRICT
            );
        )SQL"),
        QString::fromUtf8(R"SQL(
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
        )SQL"),
        QString::fromUtf8(R"SQL(
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
        )SQL"),
        QString::fromUtf8(R"SQL(
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
        )SQL"),
        QString::fromUtf8(R"SQL(
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
        )SQL"),
        QString::fromUtf8(R"SQL(
            CREATE TABLE external_competitions (
                competition_id INTEGER PRIMARY KEY,
                title TEXT NOT NULL,
                year INTEGER NOT NULL,
                UNIQUE (title, year)
            );
        )SQL"),
        QString::fromUtf8(R"SQL(
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
        )SQL")
    };
    return statements;
}

const QStringList& Schema::indexCreationStatements()
{
    static const QStringList statements = {
        QString::fromUtf8(R"SQL(
            CREATE INDEX idx_members_status
                ON members(status);
        )SQL"),
        QString::fromUtf8(R"SQL(
            CREATE INDEX idx_attendance_member
                ON meeting_attendance(member_id);
        )SQL"),
        QString::fromUtf8(R"SQL(
            CREATE INDEX idx_project_members_member
                ON project_members(member_id);
        )SQL"),
        QString::fromUtf8(R"SQL(
            CREATE INDEX idx_committee_member
                ON committee_appointments(member_id);
        )SQL"),
        QString::fromUtf8(R"SQL(
            CREATE INDEX idx_competition_participants_member
                ON competition_participants(member_id);
        )SQL")
    };
    return statements;
}

const QList<TableSchema>& Schema::requiredTables()
{
    static const QList<TableSchema> tables = {
        {"members", {"member_id", "name", "class", "phone", "email", "status", "join_date"}},
        {"meetings", {"meeting_id", "title", "type", "meeting_date", "start_time", "end_time", "location"}},
        {"meeting_attendance", {"meeting_id", "member_id"}},
        {"projects", {"project_id", "title", "status", "start_date", "target_end_date", "actual_end_date"}},
        {"project_members", {"project_id", "member_id", "role"}},
        {"committee_appointments", {"start_date", "office", "member_id", "end_date"}},
        {"activities", {"activity_id", "title", "type", "activity_date", "start_time", "end_time", "location"}},
        {"external_competitions", {"competition_id", "title", "year"}},
        {"competition_participants", {"competition_id", "member_id", "role"}}
    };
    return tables;
}

const QStringList& Schema::requiredIndexes()
{
    static const QStringList indexes = {
        "idx_members_status",
        "idx_attendance_member",
        "idx_project_members_member",
        "idx_committee_member",
        "idx_competition_participants_member"
    };
    return indexes;
}

const QString& Schema::fullSchemaSql()
{
    return FULL_SCHEMA_SQL;
}

} // namespace MSMS
