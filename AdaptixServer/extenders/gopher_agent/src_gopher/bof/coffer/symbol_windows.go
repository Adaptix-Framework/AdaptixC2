// Ref: https://github.com/RIscRIpt/pecoff

package coffer

import defwin "gopher/bof/defwin"

type SymbolParsed struct {
	defwin.Symbol
	nameString string
}

func (s *SymbolParsed) NameString() string {
	return s.nameString
}

type Symbols []*SymbolParsed
