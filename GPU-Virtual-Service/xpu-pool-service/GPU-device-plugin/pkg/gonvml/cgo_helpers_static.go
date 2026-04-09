package gonvml

import "C"

func convertSlice[T any, I any](input []T) []I {
	output := make([]I, len(input))
	for i, obj := range input {
		switch v := any(obj).(type) {
		case I:
			output[i] = v
		}
	}
	return output
}
