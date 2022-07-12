_qute_complete()
{
	local cur OPTS_ALL
	COMPREPLY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"
	case $prev in
		"--dependency-learning")
			OPTS_ALL="all outermost fewest off"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
		"--decision-heuristic")
			OPTS_ALL="VSIDS VMTF SGDB" #TODO only offer VSIDS and SGDB as long as DL is on
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
		"--model-generation")
			OPTS_ALL="off depqbf weighted"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
		"--phase-heuristic")
			OPTS_ALL="invJW qtype watcher random false true"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
		"--restarts")
			OPTS_ALL="off luby inner-outer EMA"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
		"--rrs")
			OPTS_ALL="off clauses both"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
		"--watched-literals")
			OPTS_ALL="2 3"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
		"--out-of-order-decisions")
			OPTS_ALL="off existential universal all"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
	esac
	case $cur in
		-*)
			OPTS_ALL="--initial-clause-DB-size
					--initial-term-DB-size
					--clause-DB-increment
					--term-DB-increment
					--clause-removal-ratio
					--term-removal-ratio
					--use-activity-threshold
					--LBD-threshold
					--constraint-activity-inc
					--constraint-activity-decay
					--decision-heuristic
					--restarts
					--model-generation
					--dependency-learning
					--watched-literals
					--out-of-order-decisions
					--rrs
					--no-phase-saving
					--phase-heuristic
					--partial-certificate
					-v --verbose
					--print-stats
					--machine-readable
					--trace
					-t --time-limit
					--exponent
					--scaling-factor
					--universal-penalty
					--tiebreak
					--var-activity-inc
					--var-activity-decay
					--initial-learning-rate
					--learning-rate-decay
					--learning-rate-minimum
					--lambda-factor
					--luby-restart-multiplier
					--alpha
					--minimum-distance
					--threshold-factor
					--inner-restart-distance
					--outer-restart-distance
					--restart-multiplier"
			COMPREPLY=( $(compgen -W "${OPTS_ALL[*]}" -- $cur) )
			return 0
			;;
	esac
	local IFS=$'\n'
	compopt -o filenames
	COMPREPLY=( $(compgen -f -- $cur) )
	return 0
}
complete -F _qute_complete qute
complete -F _qute_complete ./qute
