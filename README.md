# Chat GPT - command line + c library

- Fast
- Supports loading files into chat
- SQLite3 for persistence between sessions - opt in, off by default
- System prompts
- Set model parameters
- Clear chat history, or a specific message

**N.B** - Requires an [openai key](https://openai.com/blog/openai-api)

## Who is this for?
If, like me, you spend a lot of time in the terminal perhaps use something like
tmux to flip between different programs you'll probably get some mileage from 
this tool. It's like the web chatgpt interface except from the command line
and with some additional benefits like deleting individual messages from history.

## Compiling
Run:
```
make
```

If you would like to install then run:

```
make install
```


## Commands
This has been taken directly from the /help command

```
/save - Saves current chat to SQLite3 database
/autosave - Saves current chat to SQLite3 database and will save all future messages both to and from GPT
/models - Lists all openai models avalible to you
/info - Lists all current configured options
/system <cmd> - Write a system message, has a massive impact on how GPT behaves
/file <file_path> <cmd> - Load in a file and ask GPT about it!
/hist-list - List current chat history
/hist-del <msg_idx> - Delete a specific message from memory
/hist-clear - Clear all history from memory, but not SQLite3
/chat-list - List chats saved in database
/chat-load <id> - Load a previously saved chat from database
/chat-del <id> - Delete a chat from database
/chat-rename <id> <name> - Rename a chat with id <id> to <name> in database
```

## Options:
```
/set-verbose <1|0> - Prints HTTP information, streams and debug info
/set-model <name> - Swith the currently used model
/set-top_p <float> - Set nucleus sampling, where the model considers the results of the tokens with top_p probability mass
/set-presence-pen <float> - 2.0 - 2.0 Positives penalize tokens if they have already appeared in the text
/set-temperature <float> - 0.0 - 2.0 Higher values will make the output more random

/exit - Exits program
/help - Displays this message
```

## Libraries used
- [linenoise](https://github.com/antirez/linenoise) A smaller replacement for readline
- [easy-json](https://github.com/Jamesbarford/easy-json) User friendly and fast JSON parser
