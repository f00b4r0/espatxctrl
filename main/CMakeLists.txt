execute_process(COMMAND make -C ${CMAKE_CURRENT_LIST_DIR} -f ${CMAKE_CURRENT_LIST_DIR}/cmdparse.mk csources)
idf_component_register(SRCS "cmdparse.lex.c" "cmdparse.tab.c"
			"main.c" "ethernet.c" "server.c"
			INCLUDE_DIRS "")
