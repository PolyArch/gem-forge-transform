echo TASK_ID=${SGE_TASK_ID}
export LD_LIBRARY_PATH="/u/home/s/seanzw/install/lib:$LD_LIBRARY_PATH"

COMMAND_SH=${SCRATCH}/command.${SGE_TASK_ID}.sh
if [-f ${COMMAND_SH}]; then
    bash ${COMMAND_SH}
else
    echo "Command file ${COMMAND_SH} does not exist"
fi

