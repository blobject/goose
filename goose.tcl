#!/usr/bin/wish

wm title . "Goose"

set bindings_help {Keybindings:}

rename bind original_bind
proc bind {path binding action description} {
	global bindings_help
	append bindings_help "\n $description"
	original_bind $path $binding $action
}

bind . <c> {spawn foobar}        "c: Create window"
bind . <d> {pack forget [focus]} "d: Delete window"
bind . <e> {exit}                "e: Exit"

label .keybindingsInfo -text $bindings_help -anchor n -justify left
pack .keybindingsInfo -side left -anchor w -fill both

namespace eval goose {
	variable window {}
	variable next_id 1
}

proc spawn {command} {
	set win_name [join ".textBox $goose::next_id" ""]
	append goose::windows $win_name
	text $win_name -width 10
	
	if {[focus] == {.} } {
		pack $win_name -fill both -expand true -side left
	} {
		pack $win_name -fill both -expand true -side left -after [focus]
	}
	
	$win_name insert 1.0 $win_name

	incr goose::next_id
}
