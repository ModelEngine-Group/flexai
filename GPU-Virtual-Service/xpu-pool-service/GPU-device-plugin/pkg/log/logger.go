/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package log provides a rolling FileLogger.
package log

import (
	"bytes"
	"errors"
	"flag"
	"fmt"
	"os"

	"github.com/sirupsen/logrus"
)

var (
	logger         = logrus.New()
	loggingConsole = flag.Bool("logging-console", false, "enable log output, default false")
	logLevel       = flag.String("log-level", "info", "Set logging level (debug, info, warning, fatal)")
	maxSize        = flag.Int("max-size", defaultMaxSize, "maximum length of a log (MB)")
	maxBackups     = flag.Int("max-backups", defaultMaxBackups, "maximum number of log files")
	maxAge         = flag.Int("max-age", defaultMaxAge, "maximum storage duration, in days")
)

const (
	defaultMaxSize    = 20
	defaultMaxBackups = 10
	defaultMaxAge     = 30
	timestampFormat   = "2006-01-02 15:04:05.000"
)

func checkLoggerParamValid() error {
	if *maxAge <= 0 {
		return errors.New("the max-ages is invalid")
	}
	if *maxBackups <= 0 {
		return errors.New("the max-backups is invalid")
	}
	return nil
}

// InitLogging configures logging. Logs are written to a log file or stdout/stderr.
// Since logrus doesn't support multiple writers, each log stream is implemented as a hook.
func InitLogging(logName string) error {
	if err := checkLoggerParamValid(); err != nil {
		Fatalln(err)
	}

	logFileOutput := FileLogger{
		fileName:   logName,
		maxSize:    *maxSize,
		maxBackups: *maxBackups,
		maxAge:     *maxAge,
	}
	logger.SetOutput(&logFileOutput)

	// set logging level
	level, err := parseLogLevel()
	if err != nil {
		return err
	}
	logger.SetLevel(level)

	formatter := &PlainTextFormatter{TimestampFormat: timestampFormat, pid: os.Getpid()}
	logger.SetFormatter(formatter)

	if *loggingConsole {
		logConsoleHook, err := newConsoleHook(formatter)
		if err != nil {
			return fmt.Errorf("could not initialize logging to console: %v", err)
		}
		logger.AddHook(logConsoleHook)
	}
	return nil
}

// parseLogLevel parse the level of log
func parseLogLevel() (logrus.Level, error) {
	switch *logLevel {
	case "debug":
		return logrus.DebugLevel, nil
	case "info":
		return logrus.InfoLevel, nil
	case "warning":
		return logrus.WarnLevel, nil
	case "error":
		return logrus.ErrorLevel, nil
	case "fatal":
		return logrus.FatalLevel, nil
	default:
		return logrus.FatalLevel, fmt.Errorf("invalid logging level [%v]", logLevel)
	}
}

// PlainTextFormatter is a formatter to ensure formatted logging output
type PlainTextFormatter struct {
	TimestampFormat string
	pid             int
}

var _ logrus.Formatter = &PlainTextFormatter{}

// Format ensures formatted logging output
func (f *PlainTextFormatter) Format(entry *logrus.Entry) ([]byte, error) {
	b := entry.Buffer
	if entry.Buffer == nil {
		b = &bytes.Buffer{}
	}

	if _, err := fmt.Fprintf(b, "%s %d", entry.Time.Format(f.TimestampFormat), f.pid); err != nil {
		return nil, err
	}

	if len(entry.Data) != 0 {
		for key, value := range entry.Data {
			if _, err := fmt.Fprintf(b, "[%s:%v] ", key, value); err != nil {
				return nil, err
			}
		}
	}

	if _, err := fmt.Fprintf(b, "%s %s\n", getLogLevel(entry.Level), entry.Message); err != nil {
		return nil, err
	}
	return b.Bytes(), nil
}

// getLogLevel get the level of log
func getLogLevel(level logrus.Level) string {
	switch level {
	case logrus.DebugLevel:
		return "[DEBUG]:"
	case logrus.InfoLevel:
		return "[INFO]:"
	case logrus.WarnLevel:
		return "[WARNING]:"
	case logrus.ErrorLevel:
		return "[ERROR]:"
	case logrus.FatalLevel:
		return "[FATAL]:"
	default:
		return "[UNKNOWN]:"
	}
}

// Debugf ensures output of Debugf logs
func Debugf(format string, args ...interface{}) {
	logger.Debugf(format, args...)
}

// Debugln ensures output of Debugln logs
func Debugln(args ...interface{}) {
	logger.Debugln(args...)
}

// Infof ensures output of Infof logs
func Infof(format string, args ...interface{}) {
	logger.Infof(format, args...)
}

// Infoln ensures output of Infoln logs
func Infoln(args ...interface{}) {
	logger.Infoln(args...)
}

// Warningf ensures output of Warningf logs
func Warningf(format string, args ...interface{}) {
	logger.Warningf(format, args...)
}

// Warningln ensures output of Warningln logs
func Warningln(args ...interface{}) {
	logger.Warningln(args...)
}

// Errorf ensures output of Errorf logs
func Errorf(format string, args ...interface{}) {
	logger.Errorf(format, args...)
}

// Errorln ensures output of Errorln logs
func Errorln(args ...interface{}) {
	logger.Errorln(args...)
}

// Fatalf ensures output of Fatalf logs
func Fatalf(format string, args ...interface{}) {
	logger.Fatalf(format, args...)
}

// Fatalln ensures output of Fatalln logs
func Fatalln(args ...interface{}) {
	logger.Fatalln(args...)
}

// Panicln Prints Panic messages for panic
func Panicln(args ...interface{}) {
	logger.Panicln(args...)
}
