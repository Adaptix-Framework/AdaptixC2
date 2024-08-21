package server

import (
	"AdaptixServer/core/httphandler"
	"AdaptixServer/core/profile"
)

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	AdaptixServer *httphandler.TsHttpHandler
}
