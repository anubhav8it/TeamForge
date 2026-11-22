import QtQuick
import TeamForge

// A Community person's avatar. The same component as everywhere else (Avatar): photo or
// initials on their colour, from their one identity.
Avatar {
    name: person && person.name ? person.name : ""
    seed: ""
}
