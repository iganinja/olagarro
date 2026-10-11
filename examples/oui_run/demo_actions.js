// The actions the demo panel offers, apart from the panel so it shows how a script imports another.
export const actions = [
    {title: "Disk space", description: "How full each disk is, from df.", command: "df -h"},
    {title: "Who is logged in", description: "The sessions open on this machine right now.", command: "who"},
    {
        title: "Count to five, slowly",
        description: "Stands in for a backup or a build: it takes a while, and Esc stops it.",
        command: "for i in 1 2 3 4 5; do echo $i; sleep 1; done",
        confirm: "Count to five? It takes five seconds.",
    },
];
