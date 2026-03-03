<script setup lang="ts">
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
</script>

<template>
  <div class="p-8 space-y-4">
    <Button variant="default" size="sm">Large primary button</Button>

    <Card class="w-[350px]">
      <CardHeader>
        <CardTitle>Card title</CardTitle>
      </CardHeader>
      <CardContent>
        <p>Some quick example text.</p>
        <Button>Go somewhere</Button>
      </CardContent>
    </Card>
  </div>
</template>
