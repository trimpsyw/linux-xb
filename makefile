# 
# �����Ϊ��ѡ���Դ�����
# ������İ�Ȩ(����Դ�뼰�����Ʒ����汾)��һ�й������С�
# ����������ʹ�á������������
# ��Ҳ�������κ���ʽ���κ�Ŀ��ʹ�ñ����(����Դ�뼰�����Ʒ����汾)���������κΰ�Ȩ���ơ�
# =====================
# ����: ������
# ����: sunmingbao@126.com
# 

# CROSS_COMPILE=arm-linux-
export os_name:=$(shell uname)
ifeq ($(os_name),Linux)
export is_linux:=yes
else
export is_linux:=no
endif

export CC:=$(CROSS_COMPILE)gcc
export CFLAGS:=-c -O2 -Wall -fno-strict-overflow -fno-strict-aliasing
export LD:=$(CROSS_COMPILE)ld
export AR:=$(CROSS_COMPILE)ar

export RM:=rm
export MKDIR:=mkdir

export project_root_path:=$(shell pwd)
export lib_src_path:=$(project_root_path)/src
export app_code_path:=$(project_root_path)/apps
export INC_DIRS:=-I./ -I$(project_root_path)/inc

export project_target_path:=$(project_root_path)/target

export LIB_DEP_PATH:=$(project_target_path)/lib/dep
export LIB_OBJ_PATH:=$(project_target_path)/lib/obj
export APP_LIB_NAME:=app_lib
export LIB_FILE_STATIC:=$(project_target_path)/lib/$(APP_LIB_NAME).a
export LIB_FILE_DYNAMIC:=$(project_target_path)/lib/$(APP_LIB_NAME).so

export APP_CODE_DEP_PATH:=$(project_target_path)/app_code/dep
export APP_CODE_OBJ_PATH:=$(project_target_path)/app_code/obj

export LDFLAGS:=-rdynamic -L$(project_target_path)/lib/
#export LDFLAGS := $(LDFLAGS) -static
export C_LIBS:=-ldl -lpthread 

.PHONY:default prepare clean

default:prepare
#	@echo $(is_linux)
	make -C $(lib_src_path)
	make -C $(app_code_path)
	@echo build project done!

prepare:
	@$(MKDIR) -p $(project_target_path)
	@$(MKDIR) -p $(LIB_DEP_PATH)
	@$(MKDIR) -p $(LIB_OBJ_PATH)
	@$(MKDIR) -p $(APP_CODE_DEP_PATH)
	@$(MKDIR) -p $(APP_CODE_OBJ_PATH)

clean:
	$(RM) -rf $(project_target_path)
	@echo clean project done!

