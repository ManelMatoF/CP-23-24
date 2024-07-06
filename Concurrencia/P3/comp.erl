-module(comp).

-export([comp_proc/2, comp_proc/3 , decomp_proc/2, decomp_proc/3]).


-define(DEFAULT_CHUNK_SIZE, 1024*1024).

%%% File Compression

comp_proc(File, Procs) -> %% Compress file to file.ch with multiple processes
                comp_proc(File, ?DEFAULT_CHUNK_SIZE, Procs).

comp_proc(File, Chunk_Size, Procs) -> case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    create_comp_processes(Reader, Writer, Procs);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.


create_comp_processes(Reader, Writer, Procs) ->
    Parent = self(),  %% Obtener el identificador del proceso padre
    [spawn_link(fun() -> comp_loop(Reader, Writer, Parent) end) || _ <- lists:seq(1, Procs)],
    %% Esperar a que todos los procesos hayan finalizado
    wait_for_completion(Procs, Reader, Writer).

comp_loop(Reader, Writer, Parent) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_loop(Reader, Writer, Parent);
        eof ->  %% end of file, stop reader and writer
            %% Enviar mensaje de confirmación al proceso padre
            Parent ! {finished, self()};
        {error, Reason} ->
            io:format("Error reading input file: ~w~n",[Reason]),
            Reader ! stop,
            Writer ! abort
    end.

wait_for_completion(0, Reader, Writer) -> 
    Reader ! stop, 
    Writer ! stop,
    ok;  %% Todos los procesos han finalizado, parar reader writer aqui 
wait_for_completion(Procs, Reader, Writer) ->
    receive
        {finished, Pid}  ->  %% Verificar que el proceso ha finalizado correctamente
            wait_for_completion(Procs - 1, Reader, Writer)
    end.

%% File Decompression

decomp_proc(Archive, Procs) ->
    decomp_proc(Archive, string:replace(Archive, ".ch", "", trailing), Procs).

decomp_proc(Archive, Output_File, Procs) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    create_decomp_processes(Reader, Writer, Procs);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

create_decomp_processes(Reader, Writer, Procs) ->
    Parent = self(),  %% Obtener el identificador del proceso padre
    [spawn_link(fun() -> decomp_loop(Reader, Writer, Parent) end) || _ <- lists:seq(1, Procs)],
    %% Esperar a que todos los procesos hayan finalizado
    wait_for_completion_decomp(Procs, Reader, Writer).

decomp_loop(Reader, Writer, Parent) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop(Reader, Writer, Parent);
        eof ->    %% end of file => exit decompression
            Parent ! {finished, self()};
        {error, Reason} ->
            io:format("Error reading input file: ~w~n", [Reason]),
            Writer ! abort,
            Reader ! stop
    end.

wait_for_completion_decomp(0, Reader, Writer) -> 
    Reader ! stop, 
    Writer ! stop,
    ok;  %% Todos los procesos han finalizado, parar reader writer aqui 

wait_for_completion_decomp(Procs, Reader, Writer) ->
    receive
        {finished, Pid}  ->  %% Verificar que el proceso ha finalizado correctamente
            wait_for_completion_decomp(Procs - 1, Reader, Writer)
    end.