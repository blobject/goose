#!/usr/bin/wish

wm title . "Goose"

set bindings_help {Keybindings:}

# Add descriptions to bind
rename bind original_bind
proc bind {path binding action description} {
	global bindings_help
	append bindings_help "\n $description"
	original_bind $path $binding $action
}

proc focus_next {} {
	set windows [pack content .frame]
	set idx [lsearch -exact $windows [focus]]
	if {$idx != -1} {
		focus [lindex $windows [expr {($idx + 1) % [llength $windows]}]]
	}
}

proc delete_current {} {
	set current [focus]
	focus_next
	pack forget $current
}


bind . <c> {spawn foobar}        "c: Create window"
bind . <d> {delete_current}      "d: Delete window"
bind . <j> {focus_next}          "j: Focus next window"
bind . <e> {exit}                "e: Exit"

label .keybindingsInfo -text $bindings_help -anchor n -justify left
pack .keybindingsInfo -side left -anchor w -fill y

frame .frame
pack .frame -fill both -expand 1

namespace eval goose {
	variable next_id 1
}

proc spawn {command} {
	set win_name ".frame.textBox$goose::next_id"
	text $win_name -width 10
	
	if {[focus] == {.} } {
		pack $win_name -fill both -expand true -side left
	} {
		pack $win_name -fill both -expand true -side left -after [focus]
	}
	
	# Add content to the window
	$win_name insert 1.0 $win_name

	incr goose::next_id
}
