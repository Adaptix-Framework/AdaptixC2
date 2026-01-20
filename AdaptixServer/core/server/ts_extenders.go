package server

func (ts *Teamserver) TsExtenderDataSave(extenderName string, key string, data []byte) error {
	return ts.DBMS.DbExtenderDataSave(extenderName, key, data)
}

func (ts *Teamserver) TsExtenderDataLoad(extenderName string, key string) ([]byte, error) {
	return ts.DBMS.DbExtenderDataLoad(extenderName, key)
}

func (ts *Teamserver) TsExtenderDataDelete(extenderName string, key string) error {
	return ts.DBMS.DbExtenderDataDelete(extenderName, key)
}

func (ts *Teamserver) TsExtenderDataKeys(extenderName string) ([]string, error) {
	return ts.DBMS.DbExtenderDataKeys(extenderName)
}

func (ts *Teamserver) TsExtenderDataDeleteAll(extenderName string) error {
	return ts.DBMS.DbExtenderDataDeleteAll(extenderName)
}
