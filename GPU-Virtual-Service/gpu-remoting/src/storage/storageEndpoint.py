import socket
import errno
# import multiprocessing as mp
import torch.multiprocessing as mp
from dataFeeder import DataFeeder
from common import DataloaderInfo, BatchInfo, get_server_ep, logging, send_msg, recv_msg, EPOCH_REQ, EPOCH_STOP, DATALOADER_REQ

def handle_client(client_socket, client_addr, rank):
    logging.info(f"Accepted connection from {client_addr}")
    datafeeders = [None, None]
    client_ident = None

    try:
        while True:
            infoObj = recv_msg(client_socket)
            if infoObj is None:
                break

            request_type = infoObj[0]
            is_train = infoObj[1]
            if request_type == DATALOADER_REQ:
                if client_ident is None:
                    client_ident = infoObj[2]
                dataloader_info = infoObj[3]
                logging.debug(f"[{client_ident}] new dataloader info: {dataloader_info}")

                datafeeder = DataFeeder(client_ident, rank, dataloader_info)
                datafeeders[is_train] = datafeeder
                # create the shared memory queue
                send_msg(client_socket, datafeeder.get_batch_info(is_first=True))

            elif request_type == EPOCH_REQ:
                client_epoch_idx = infoObj[2]
                logging.info(f"[{client_ident}] new epoch request (train={is_train}, epoch={client_epoch_idx})")
                datafeeders[is_train].reset(client_epoch_idx)
                send_msg(client_socket, datafeeders[is_train].get_batch_info(is_first=False))
                datafeeders[is_train].start_load_data()
            elif request_type == EPOCH_STOP:
                client_batch_idx = infoObj[2]
                logging.info(f"[{client_ident}] epoch stop request (train={is_train}, client_batch_idx={client_batch_idx})")
                datafeeders[is_train].stop_load_data(client_batch_idx)
            else:
                logging.error(f"Received unknown message: {infoObj}")
    except socket.error as e:
        if e.errno == errno.ECONNRESET:
            pass
        else:
            logging.error(f"Unexpected socket error: {e}")
    except Exception as e:
        logging.error(f"Exception: {e}")
    finally:
        for datafeeder in datafeeders:
            if datafeeder is not None:
                datafeeder.stop_load_data()
        logging.info(f"Connection from client#{client_ident}{client_addr} closed")
        client_socket.close()



def listener(server_address):
    conn_num = 0
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        server_socket.bind(server_address)
    except OSError as e:
        if e.errno == errno.EADDRINUSE:
            logging.error(f"Address {server_address[0]}:{server_address[1]} is already in use")
            return
        else:
            logging.error(f"Unexpected error: {e}")
            return
    server_socket.listen()
    logging.info(f"Listening on {server_address[0]}:{server_address[1]}")
    try:
        while True:
            client_socket, client_addr = server_socket.accept()
            conn_num += 1
            process = mp.Process(target=handle_client, args=(client_socket, client_addr, conn_num))
            process.start()
            client_socket.close() # close the socket in parent process, let the child process handle dataloading
    except KeyboardInterrupt:
        logging.info("Shutting down...")
    except Exception as e:
        logging.error(f"Error: {e}")
    finally:
        server_socket.close()

if __name__ == "__main__":
    listener(get_server_ep())