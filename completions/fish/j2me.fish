function __j2me_workspace_root
    set -l dir (pwd)
    while test "$dir" != /
        if test -f "$dir/j2me.workspace.json"
            echo "$dir"
            return 0
        end
        set dir (dirname "$dir")
    end
    return 1
end

function __j2me_projects
    set -l ws (__j2me_workspace_root)
    test -n "$ws"; or return 0
    test -d "$ws/projects"; or return 0

    for project in "$ws"/projects/*
        test -d "$project"; and test -f "$project/j2me.proj.json"; and basename "$project"
    end
end

function __j2me_tools
    set -l ws (__j2me_workspace_root)
    test -n "$ws"; or return 0
    test -d "$ws/tools"; or return 0

    for tool in "$ws"/tools/*
        test -d "$tool"; and test -f "$tool/j2me.tool.yml"; and basename "$tool"
    end
end

function __j2me_profiles
    for file in j2me.profile-*.json
        test -f "$file"; or continue
        string replace -r '^j2me\.profile-(.*)\.json$' '$1' "$file"
    end
end

function __j2me_jdks
    set -l ws (__j2me_workspace_root)
    test -n "$ws"; or return 0
    test -d "$ws/sdk/jdk"; or return 0

    for jdk in "$ws"/sdk/jdk/*
        test -d "$jdk"; and basename "$jdk"
    end
end

complete -c j2me -f

complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l help -d 'Show help'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -s h -d 'Show help'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l version -d 'Show version'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -s V -d 'Show version'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l update -d 'Check GitHub Releases and self-update'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l workspace -d 'Manage workspace'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l project -d 'Manage projects'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -s P -d 'Manage projects'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l sdk -d 'Manage SDK packages'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -s S -d 'Manage SDK packages'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l tools -d 'Run bundled tools'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -s T -d 'Run bundled tools'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l build -d 'Build project'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l emulator -d 'Manage emulator'
complete -c j2me -n 'not __fish_seen_subcommand_from --workspace --project -P --sdk -S --tools -T --build --emulator --run --help -h --version -V --update' -l run -d 'Run project in emulator'

complete -c j2me -n '__fish_seen_subcommand_from --workspace; and not __fish_seen_subcommand_from setup update remove info' -a 'setup' -d 'Create workspace'
complete -c j2me -n '__fish_seen_subcommand_from --workspace; and not __fish_seen_subcommand_from setup update remove info' -a 'update' -d 'Update workspace templates'
complete -c j2me -n '__fish_seen_subcommand_from --workspace; and not __fish_seen_subcommand_from setup update remove info' -a 'remove' -d 'Remove workspace-owned content'
complete -c j2me -n '__fish_seen_subcommand_from --workspace; and not __fish_seen_subcommand_from setup update remove info' -a 'info' -d 'Show workspace info'
complete -c j2me -n '__fish_seen_subcommand_from --workspace; and __fish_seen_subcommand_from remove' -l yes -d 'Skip remove confirmation'

complete -c j2me -n '__fish_seen_subcommand_from --project -P; and not __fish_seen_subcommand_from create remove list properties' -a 'create' -d 'Create project'
complete -c j2me -n '__fish_seen_subcommand_from --project -P; and not __fish_seen_subcommand_from create remove list properties' -a 'remove' -d 'Remove project'
complete -c j2me -n '__fish_seen_subcommand_from --project -P; and not __fish_seen_subcommand_from create remove list properties' -a 'list' -d 'List projects'
complete -c j2me -n '__fish_seen_subcommand_from --project -P; and not __fish_seen_subcommand_from create remove list properties' -a 'properties' -d 'Edit project properties'
complete -c j2me -n '__fish_seen_subcommand_from --project -P; and __fish_seen_subcommand_from remove properties' -a '(__j2me_projects)'

complete -c j2me -n '__fish_seen_subcommand_from --sdk -S; and not __fish_seen_subcommand_from setup list' -a 'setup' -d 'Create SDK package directories'
complete -c j2me -n '__fish_seen_subcommand_from --sdk -S; and not __fish_seen_subcommand_from setup list' -a 'list' -d 'List installed SDK packages'

complete -c j2me -n '__fish_seen_subcommand_from --tools -T; and not __fish_seen_subcommand_from list update' -a 'list' -d 'List tools'
complete -c j2me -n '__fish_seen_subcommand_from --tools -T; and not __fish_seen_subcommand_from list update' -a 'update' -d 'Update tool'
complete -c j2me -n '__fish_seen_subcommand_from --tools -T; and not __fish_seen_subcommand_from list update' -a '(__j2me_tools)'

complete -c j2me -n '__fish_seen_subcommand_from --emulator; and not __fish_seen_subcommand_from setup' -a 'setup' -d 'Configure emulator directories'

complete -c j2me -n '__fish_seen_subcommand_from --build' -a '(__j2me_projects)'
complete -c j2me -n '__fish_seen_subcommand_from --build' -l profile -d 'Select build profile' -r -a '(__j2me_profiles)'
complete -c j2me -n '__fish_seen_subcommand_from --build' -l jdk -d 'Select JDK version' -r -a '(__j2me_jdks)'
complete -c j2me -n '__fish_seen_subcommand_from --build' -l run-after-build -d 'Run emulator after build'

complete -c j2me -n '__fish_seen_subcommand_from --run' -a '(__j2me_projects)'
complete -c j2me -n '__fish_seen_subcommand_from --run' -l profile -d 'Select run profile' -r -a '(__j2me_profiles)'
