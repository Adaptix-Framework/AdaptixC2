// Ref: https://github.com/RIscRIpt/pecoff

package coffer

import defwin "gopher/bof/defwin"

// Symbol embedds a Symbol struct
// and stores unexported fields of parsed data of a symbol.
type SymbolParsed struct {
	defwin.Symbol
	nameString string
}

// NameString returns a string represntation of the field `Name`.
func (s *SymbolParsed) NameString() string {
	return s.nameString
}

// Symbols represents a collection of pointers to SymbolParsed objects.
type Symbols []*SymbolParsed
