void i2x_exec_segment(struct i2x_segment *segment, uint8_t *reg,
				size_t reg_width)
{
	for (size_t i = 0; i < segment->msgset.nmsgs; ++i) {
		struct i2c_msg *msg = segment->msgset.msgs + i;
		printf("{addr=XXXX  flags=%04x  len=%4d  buf=[",
			msg->flags & ~F_MSG_REG, msg->len);

		if (msg->flags & I2C_M_RD) {
			for (size_t j = 0; j < msg->len; ++j)
				printf(" ..");
		} else {
			for (size_t j = 0; j < msg->len; ++j) {
				if (j < reg_width && msg->flags & F_MSG_REG)
					printf(" %02hhx", reg[j]);
				else
					printf(" %02hhx", msg->buf[j]);
			}
		}
		puts(" ]}");
	}
	if (segment->delay)
		printf("--- %4d ms ", segment->delay);
	puts("---"); /* stop */
}

void i2x_exec_segment_list(struct i2x_list *segment_list, uint8_t *reg,
				size_t reg_width)
{
	i2x_list_foreach (i2x_segment, segment, segment_list)
		i2x_exec_segment(segment, reg, reg_width);
}

void i2x_exec_cmd(struct i2x_cmd *cmd)
{
	struct i2x_list *reg_spec = cmd->reg_spec;
	struct i2x_list *segment_list = cmd->segment_list;

	if (!reg_spec) {
		i2x_exec_segment_list(segment_list, NULL, 0);
		return;
	}

	assert(reg_spec->len);
	size_t width = i2x_list_get(i2x_regrange, reg_spec, 0)->width;

	uint8_t *reg = malloc(width);
	assert(reg);
	i2x_list_foreach (i2x_regrange, range, reg_spec) {
		memcpy(reg, range->start, width);
		do {
			i2x_exec_segment_list(segment_list, reg, width);
			incn(reg, width, range->big_endian);
		} while(memcmp(reg, range->stop, width));
	}
	free(reg);
}

void i2x_exec_cmd_list(struct i2x_list *cmd_list)
{
	i2x_list_foreach (i2x_cmd, cmd, cmd_list)
		i2x_exec_cmd(cmd);
}
