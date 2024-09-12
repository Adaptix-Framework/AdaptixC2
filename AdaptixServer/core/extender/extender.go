package extender

type Teamserver interface {
}

type AdaptixExtender struct {
	ts Teamserver
}

func NewExtender(teamserver Teamserver) *AdaptixExtender {
	return &AdaptixExtender{
		ts: teamserver,
	}
}

func (ex *AdaptixExtender) InitPlugins(extenderFile string) {
	// TODO init plugin
}
